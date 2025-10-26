#ifndef TASK_MANAGER_H_
#define TASK_MANAGER_H_

namespace npr_core {

class TaskManager {
  using TaskFn = std::function<bool()>;

 public:
  void Add(TaskFn task) { pending_.push_back(std::move(task)); }

  void Process() {
    current_.swap(pending_);
    pending_.clear();

    for (auto& task : current_)
      if (!task()) pending_.push_back(std::move(task));
  }

 private:
  std::vector<TaskFn> current_;
  std::vector<TaskFn> pending_;
};

}  // namespace npr_core

#endif  // TASK_MANAGER_H_
