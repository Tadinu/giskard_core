/*
 * Copyright (C) 2017 Georg Bartels <georg.bartels@cs.uni-bremen.de>
 * 
 * This file is part of giskard.
 * 
 * giskard is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef GISKARD_CORE_ROBOT_HPP
#define GISKARD_CORE_ROBOT_HPP

#include <urdf_model/model.h>
#include <giskard_core/specifications.hpp>
#include <set>
#include <algorithm>


namespace giskard_core
{
  inline VectorSpecPtr to_vector_spec(const urdf::Vector3& vector)
  {
    return vector_constructor_spec(
        double_const_spec(vector.x), double_const_spec(vector.y), double_const_spec(vector.z));
  }

  inline RotationSpecPtr to_quaternion_spec(const urdf::Rotation& rot)
  {
    return quaternion_spec(rot.x, rot.y, rot.z, rot.w);
  }

  inline FrameSpecPtr to_frame_spec(const urdf::Pose& pose)
  {
    return frame_constructor_spec(to_vector_spec(pose.position), to_quaternion_spec(pose.rotation));
  }

  class Robot
  {
    public:
      // TODO: move the beef of this constructor into an init function
      Robot(urdf::Model& robot_model, const std::string& root_link,
          const std::vector<std::string>& tip_links, 
          const std::map<std::string, double> weights,
          const std::map<std::string, double> thresholds) :
        robot_model_( robot_model ), weights_( weights ), thresholds_( thresholds ),
        root_link_( root_link )
      {
        robot_model_.initTree(parent_link_tree_);

        for (size_t i=0; i<tip_links.size(); ++i)
          init_kinematic_chain(root_link, tip_links[i]);
      }

      Robot(urdf::Model& robot_model, const std::string& root_link,
          const std::vector< std::pair<std::string, std::string> >& chain_links,
          const std::map<std::string, double> weights,
          const std::map<std::string, double> thresholds) :
        robot_model_( robot_model ), weights_( weights ), thresholds_( thresholds ),
        root_link_( root_link )
      {
        robot_model_.initTree(parent_link_tree_);
        for (auto const & chain: chain_links)
          init_kinematic_chain(chain.first, chain.second);
      }

      FrameSpecPtr get_fk_spec(const std::string& root_link, const std::string& tip_link) const
      {
        return get_fk_spec(std::make_pair(root_link, tip_link));
      }

      FrameSpecPtr get_fk_spec(const std::pair<std::string, std::string>& chain_links) const
      {
        if (fk_map_.find(chain_links) == fk_map_.end())
          throw std::runtime_error("Could not find fk specification for chain ('" + chain_links.first + ", " + chain_links.second + ").");

        return fk_map_.find(chain_links)->second;
      }

      const std::string& get_root_link() const
      {
        return root_link_;
      }

      std::vector<ControllableConstraintSpec> get_controllable_constraints() const
      {
        std::vector<ControllableConstraintSpec> specs;

        for (auto const & pair: controllable_map_)
          specs.push_back(pair.second);

        std::sort(specs.begin(), specs.end(),
             [](const ControllableConstraintSpec & a, const ControllableConstraintSpec & b) -> bool
             {
                 return a.input_number_ < b.input_number_;
             });

        return specs;
      }

      std::vector<HardConstraintSpec> get_hard_constraints() const
      {
        std::vector<HardConstraintSpec> specs;

        for (auto const & pair: hard_map_)
          specs.push_back(pair.second);

        return specs;
      }

      std::vector<ScopeEntry> get_scope() const
      {
        std::vector<ScopeEntry> scope;
        
        for (auto const & fk_entry: fk_map_)
          scope.push_back(ScopeEntry(fk_entry.first.second, fk_entry.second));

        return scope;
      }

      DoubleInputSpecPtr get_joint(const std::string& joint_name) const
      {
        if (joint_map_.find(joint_name) == joint_map_.end())
          throw std::runtime_error("Could not find joint with name '" + joint_name + "'.");

        return joint_map_.find(joint_name)->second;
      }

      size_t get_number_of_joints() const
      {
        return joint_map_.size();
      }

      static std::string& default_joint_weight_key()
      {
        static std::string result{"default_joint_weight"};
        return result;
      }

      static std::string& default_joint_velocity_key()
      {
        static std::string result{"default_joint_velocity"};
        return result;
      }

      std::vector<std::string> chain_joint_names(const std::string& root, const std::string& tip, bool add_fixed_joints=true) const
      {
        std::vector<std::string> chain_joints;

        std::string current_link_name = tip;
        while (current_link_name.compare(root) != 0)
        {
          if (robot_model_.links_.find(current_link_name) == robot_model_.links_.end() )
            throw std::runtime_error("Could not find link with name '" + current_link_name + "'.");

          if (!robot_model_.links_.find(current_link_name)->second)
            throw std::runtime_error("Link associated with name '" + current_link_name + "' is an empty shared_ptr.");

          urdf::JointSharedPtr parent_joint =
            robot_model_.links_.find(current_link_name)->second->parent_joint;

          if (!parent_joint)
            throw std::runtime_error("Parent joint of link with name '" + current_link_name + "' is an empty shared_ptr.");

          if (add_fixed_joints || (parent_joint->type != urdf::Joint::FIXED))
            chain_joints.push_back(parent_joint->name);

          if (parent_link_tree_.find(current_link_name) == parent_link_tree_.end())
            throw std::runtime_error("Could not find parent link of link with name '" + current_link_name + "'.");

          current_link_name = parent_link_tree_.find(current_link_name)->second;
        }

        std::reverse(chain_joints.begin(), chain_joints.end());
        return chain_joints;
      }

      bool is_continuous_joint(const std::string& joint_name) const
      {
        return robot_model_.joints_.find(joint_name)->second->type == urdf::Joint::CONTINUOUS;
      }

      std::set<std::string> continuous_joints_names(const std::string& root, const std::string& tip) const
      {
        std::set<std::string> result;
        std::vector<std::string> joint_names = chain_joint_names(root, tip, false);
        for (const auto & joint_name: joint_names)
          if (robot_model_.joints_.find(joint_name)->second->type == urdf::Joint::CONTINUOUS)
            result.insert(joint_name);

        return result;
      }

    protected:
      urdf::Model& robot_model_;
      std::map<std::string, std::string> parent_link_tree_;
      std::map< std::pair<std::string, std::string>, FrameSpecPtr> fk_map_;
      std::map<std::string, ControllableConstraintSpec> controllable_map_;
      std::map<std::string, HardConstraintSpec> hard_map_;
      std::map<std::string, DoubleInputSpecPtr> joint_map_;
      std::map<std::string, double> weights_;
      std::map<std::string, double> thresholds_;
      std::string root_link_;

      void init_kinematic_chain(const std::string& root, const std::string& tip)
      {
        std::vector<std::string> moveable_joints_names = chain_joint_names(root, tip, false);
        std::vector<std::string> all_joints_names = chain_joint_names(root, tip, true);
        // create and add input expressions for new moveable joints
        for (size_t i=0; i<moveable_joints_names.size(); ++i)
          if (joint_map_.find(moveable_joints_names[i]) == joint_map_.end())
            joint_map_.insert(std::pair<std::string, DoubleInputSpecPtr>(moveable_joints_names[i], input(joint_map_.size())));
        // create and add frame expression for new kinematic chain
        std::vector<FrameSpecPtr> joint_transforms = { frame_constructor_spec() };
        for (size_t i=0; i<all_joints_names.size(); ++i)
        {
          if ( robot_model_.joints_.find(all_joints_names[i]) == robot_model_.joints_.end() )
            throw std::runtime_error("Could not find joint with name '" + all_joints_names[i] + "'.");

          std::vector<FrameSpecPtr> new_transforms = extract_joint_transforms(
              robot_model_.joints_.find(all_joints_names[i])->second);

          joint_transforms.insert(joint_transforms.end(), new_transforms.begin(), new_transforms.end());
        }
        fk_map_.insert(std::make_pair(std::make_pair(root, tip), cached_frame(frame_multiplication_spec(joint_transforms))));
        // create and add new controllable constraints
        for (size_t i=0; i<moveable_joints_names.size(); ++i)
          if (controllable_map_.find(moveable_joints_names[i]) == controllable_map_.end())
          {
            ControllableConstraintSpec spec;
            spec.name_ = moveable_joints_names[i];
            spec.input_number_ = get_joint(spec.name_)->get_input_num();
            spec.weight_ = double_const_spec(get_weight(spec.name_));
            double vel_limit = get_velocity_limit(spec.name_);
            spec.lower_ = double_const_spec(-vel_limit);
            spec.upper_ = double_const_spec(vel_limit);

            controllable_map_.insert(std::make_pair(spec.name_, spec));
          }
        // create and new hard constraints
        for (size_t i=0; i<moveable_joints_names.size(); ++i)
          if (hard_map_.find(moveable_joints_names[i]) == hard_map_.end())
          {
            if ( robot_model_.joints_.find(moveable_joints_names[i]) == robot_model_.joints_.end() )
              throw std::runtime_error("Could not find joint with name '" + moveable_joints_names[i] + "'.");
            urdf::JointConstSharedPtr joint = robot_model_.joints_.find(moveable_joints_names[i])->second;
            if ( joint->type == urdf::Joint::REVOLUTE || joint->type == urdf::Joint::PRISMATIC )
            {
              HardConstraintSpec spec;
              spec.expression_ = get_joint(moveable_joints_names[i]);
              spec.lower_ = double_sub_spec({double_const_spec(joint->limits->lower), spec.expression_});
              spec.upper_ = double_sub_spec({double_const_spec(joint->limits->upper), spec.expression_});
              hard_map_.insert(std::make_pair(moveable_joints_names[i], spec));
            }
          }

      }

      std::vector<FrameSpecPtr> extract_joint_transforms(urdf::JointSharedPtr& joint) 
      {
        if ( !joint )
          throw std::runtime_error("Given joint is an empty shared_ptr.");

        std::vector<FrameSpecPtr> frame_specs;
        
        // translate fixed origin pose
        frame_specs.push_back(to_frame_spec(joint->parent_to_joint_origin_transform));
        
        // translate actual joint transform
        switch( joint->type ) 
        {
          case urdf::Joint::FIXED:
            // no-op
            break;
          case urdf::Joint::PRISMATIC:
            frame_specs.push_back(frame_constructor_spec(
                vector_double_mul(to_vector_spec(joint->axis), get_joint(joint->name))));
            break;
          case urdf::Joint::REVOLUTE: case urdf::Joint::CONTINUOUS:
            frame_specs.push_back(frame_constructor_spec(
                  vector_constructor_spec(),
                  axis_angle_spec(to_vector_spec(joint->axis), get_joint(joint->name))));
            break;
          case urdf::Joint::PLANAR: case urdf::Joint::FLOATING: case urdf::Joint::UNKNOWN:
            throw std::runtime_error("Joint with name '" + joint->name + "' has unsupported type.");
          default:
            throw std::runtime_error("Joint with name '" + joint->name + "' has unmodeled type.");
        }

        return frame_specs;
      }

      double get_velocity_limit(const std::string& joint_name) const
      {
        // TODO: guard against broken robot model; probably best to write some convenience getters for robot_model_
        double urdf_limit = robot_model_.getJoint(joint_name)->limits->velocity;
        double result = urdf_limit;
        if (thresholds_.count(joint_name) != 0)
          result = thresholds_.find(joint_name)->second;
        else if (thresholds_.count(default_joint_velocity_key()) != 0)
          result = thresholds_.find(default_joint_velocity_key())->second;

        if (result > urdf_limit)
          throw std::runtime_error("Came up with velocity limit faster than limit from URDF for joint '" + joint_name + "'.");

        return result;
      }

      double get_weight(const std::string& joint_name) const
      {
        double result = 0.0;
        if (weights_.find(joint_name) != weights_.end())
          result = weights_.find(joint_name)->second;
        else if (weights_.find(default_joint_weight_key()) != weights_.end())
          result = weights_.find(default_joint_weight_key())->second;
        else
          throw std::runtime_error("Could not find weight for joint '" + joint_name + "'.");

        if (result < 0)
          throw std::runtime_error("Came up with a joint weight below zero for joint '" + joint_name + "'.");
        return result;
      }
  };

}

#endif // GISKARD_CORE_ROBOT_HPP
