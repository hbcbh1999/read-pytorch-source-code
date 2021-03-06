#include "torch/csrc/autograd/saved_variable.h"

#include "torch/csrc/autograd/function.h"

using namespace at;

namespace torch { namespace autograd {

// 就是 把 Variable 的属性全都搞过来！！！！！！！！！！！
// 逻辑是这样：
// 用 Conv 举例： ConvForward 的输入， 以 SavedVariable 保存到 ConvBackward 中！！！！！！！！ saved_for ConvBackward
// 这时， variable.grad_fn 是不等于 saved_for 的
// 所以 SavedVariable 的 grad_fn 设置成 variable 的 grad_fn
// 如果 variable.grad_fn 与 saved_for 相等，这种是什么情况呢？ 就是 计算 sofmax 导数的时候！！！！！！！！！！！！！！！
// SavedVariable 是 函数 中保存的 Variable， 用来计算 反向传导时的梯度的！！！！！！！！！！！！！！！！
// saved_for ， 保存的 Variable 是要给谁用的
// 感觉这个应该和 高阶导数有点关系
SavedVariable::SavedVariable(const Variable& variable, Function* saved_for)
  : SavedVariable() {
  if (!variable.defined()) {
    return;
  }
  // data 搞过来，这样， data 底层的引用计数又 加了 一
  data = variable.data();
  requires_grad = variable.requires_grad();
  is_volatile = variable.is_volatile();
  expected_version = variable.current_version();
  version = variable.get()->version_counter.save();
  has_grad_fn = variable.grad_fn() != nullptr;
  output_nr = variable.output_nr();
  if (!has_grad_fn) {
    grad_accumulator = variable.grad_accumulator();
  }
  if (variable.grad_fn().get() != saved_for) {
    _grad_fn = variable.grad_fn();
  }
  if (variable.is_view()) {
    base = variable.base();
  }
  if (variable.tracing_state()) {
    tracing_state.reset(new jit::tracer::ValueTracingState(*variable.tracing_state()));
  }
}

auto SavedVariable::unpack(std::shared_ptr<Function> saved_for) const -> Variable {
  if (!data.defined()) {
    if (version.defined()) {
      throw std::runtime_error(ERR_BACKWARD_TWICE);
    }
    return Variable();
  }

  if (version.is_modified()) {
    throw std::runtime_error(
        "one of the variables needed for gradient computation has been "
        "modified by an inplace operation");
  }

  auto flags = VarFlags(requires_grad, is_volatile);
  auto grad_fn = _grad_fn;
  if (has_grad_fn && !grad_fn) {
    if (!saved_for) {
      // If saving the grad_fn would create a circular reference, then it must
      // be passed in to the unpack function.
      throw std::runtime_error("No grad_fn for non-leaf saved variable");
    }
    grad_fn = std::move(saved_for);
  }

  Variable var;
  if (base.defined()) {
    var = make_variable_view(base, data, flags, output_nr, std::move(grad_fn));
  } else {
    var = make_variable(data, flags, output_nr, std::move(grad_fn));
  }
  var.version_counter() = version;

  // If a Variable is a leaf (no grad_fn saved), and it requires_grad, then we
  // should have saved the grad accumulator. Even if the Variable no longer
  // alive, the accumulator should be kept alive by the references in the graph).
  if (requires_grad && !var.grad_fn() && grad_accumulator.expired())
    throw std::logic_error("No grad accumulator for a saved leaf!");
  var.get()->grad_accumulator = grad_accumulator;
  if (tracing_state)
    var.tracing_state().reset(new jit::tracer::ValueTracingState(*tracing_state));

  return var;
}

auto SavedVariable::unpack_data(std::shared_ptr<Function> saved_for) const -> Tensor {
  auto var = unpack(saved_for);
  if (var.defined()) {
    return var.data();
  }
  return Tensor();
}


const char* ERR_BACKWARD_TWICE =
    "Trying to backward through the graph a second time, but the buffers have "
    "already been freed. Specify retain_graph=True when calling backward "
    "the first time.";

}} // namespace torch::autograd
