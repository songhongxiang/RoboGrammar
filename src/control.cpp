#include <iostream>
#include <robot_design/control.h>

namespace robot_design {

PDController::PDController(const Robot &robot, Simulation &sim)
    : robot_(robot), sim_(sim) {
  Index robot_idx = sim_.findRobotIndex(robot);
  int dof_count = sim_.getRobotDofCount(robot_idx);
  kp_ = VectorX::Zero(dof_count);
  kd_ = VectorX::Zero(dof_count);
  pos_target_ = VectorX::Zero(dof_count);
  vel_target_ = VectorX::Zero(dof_count);
}

void PDController::update() {
  Index robot_idx = sim_.findRobotIndex(robot_);
  sim_.getJointPositions(robot_idx, pos_);
  sim_.getJointVelocities(robot_idx, vel_);
  torque_ = kp_.cwiseProduct(pos_target_ - pos_) +
            kd_.cwiseProduct(vel_target_ - vel_);
  sim_.addJointTorques(robot_idx, torque_);
}

MPCController::MPCController(const Robot &robot, Simulation &sim, int horizon,
                             int period, const MakeSimFunction &make_sim_fn,
                             const ObjectiveFunction &objective_fn,
                             int thread_count)
    : robot_(robot), sim_(sim), horizon_(horizon), period_(period),
      objective_fn_(objective_fn), thread_pool_(thread_count) {
  Index robot_idx = sim_.findRobotIndex(robot);
  int dof_count = sim_.getRobotDofCount(robot_idx);

  // Create independent simulation instances for finite differencing
  int instance_count = 2 * dof_count * horizon;
  sim_instances_.reserve(instance_count);
  for (int i = 0; i < instance_count; ++i) {
    sim_instances_.push_back(make_sim_fn());
  }
  sim_results_.resize(instance_count);

  // Define initial input trajectory
  inputs_ = MatrixX::Zero(dof_count, horizon);
}

void MPCController::update() {
  // Assume the robot index is the same in every simulation instance
  Index robot_idx = sim_.findRobotIndex(robot_);

  for (int i = 0; i < sim_instances_.size(); ++i) {
    sim_results_[i] = thread_pool_.enqueue(&MPCController::runSimulation, this, i);
  }

  for (auto &sim_result : sim_results_) {
    std::cout << sim_result.get() << std::endl;
  }
}

Scalar MPCController::runSimulation(int sim_index) {
  sim_instances_[sim_index]->saveState();
  for (int j = 0; j < horizon_ * period_; ++j) {
    sim_instances_[sim_index]->step();
  }
  Scalar objective_value = objective_fn_(*sim_instances_[sim_index]);
  sim_instances_[sim_index]->restoreState();
  return objective_value;
}

}  // namespace robot_design
