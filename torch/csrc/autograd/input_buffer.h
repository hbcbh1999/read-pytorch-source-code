#pragma once

// The InputBuffer class accumulates a list of Variables for use by a
// function. It implements logic to avoid modifying the passed
// values in-place (adding an input twice will accumulate the result).
// This behaviour needed and used only in backward graphs.


// 在计算梯度的时候， grad_fn 的 输入可能从 很多条路径上累积过来
// 这个类就是用来 累积 grad_fn 的输入

#include <Python.h>
#include <vector>
#include <utility>
#include <memory>
#include <ATen/ATen.h>

#include "torch/csrc/autograd/variable.h"

namespace torch { namespace autograd {

struct InputBuffer {
  // size 表示 有几个 <Variable, int> pair
  explicit InputBuffer(size_t size);
  InputBuffer(const InputBuffer& other) = delete;
  InputBuffer(InputBuffer&& other) = default;

  // Accumulates the variable at a specified index.
  void add(size_t idx, Variable var);

  int device() const;

  // Returns the inputs as a list of variables. Destroys given InputBuffer.
  static std::vector<Variable> variables(InputBuffer&& buffer);

private:
  // (Variable, version at save)
  // Variables, pair 中的 int 代表 version
  std::vector<std::pair<Variable, int>> buffer;
};

}}  // namespace torch::autograd