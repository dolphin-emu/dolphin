// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <type_traits>

#include <QAbstractSlider>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>

template <typename T>
struct MemberFunctionArgType;

template <typename R, typename C, typename A>
struct MemberFunctionArgType<R (C::*)(A)>
{
  using ArgType = A;
};

template <typename R, typename C, typename A>
struct MemberFunctionArgType<R (C::*const)(A)>
{
  using ArgType = A;
};

template <typename ControlClass, typename EnableIf = void>
struct BindableControlTrait;

template <>
struct BindableControlTrait<QCheckBox>
{
  static constexpr auto SetFunc = &QCheckBox::setChecked;
  static constexpr auto SignalFunc = &QCheckBox::toggled;
};

template <>
struct BindableControlTrait<QComboBox>
{
  static constexpr auto SetFunc = &QComboBox::setCurrentIndex;  // TODO: handle out-of-range values
  static constexpr auto SignalFunc = static_cast<void (QComboBox::*)(int)>(&QComboBox::activated);
};

void ButtonGroupSetChecked(QButtonGroup* button_group, int id)
{
  button_group->button(id)->setChecked(true);
}

template <>
struct BindableControlTrait<QButtonGroup>
{
  static decltype(&ButtonGroupSetChecked) SetFunc;
  static constexpr auto SignalFunc =
      static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked);
};
// TODO: why doesn't it work with constexpr? macOS gives me an undefined symbol error
auto BindableControlTrait<QButtonGroup>::SetFunc = &ButtonGroupSetChecked;

template <typename T>
struct BindableControlTrait<
    T, typename std::enable_if<std::is_base_of<QAbstractSlider, T>::value>::type>
{
  static constexpr auto SetFunc = &QAbstractSlider::setValue;
  static constexpr auto SignalFunc = &QAbstractSlider::valueChanged;
};

template <typename Ret, typename Base, typename T, typename... Args>
Ret CallWithContext(Ret (Base::*member_function)(Args...), T* context, Args... args)
{
  return (context->*member_function)(std::forward<Args>(args)...);
}

template <typename F, typename T, typename... Args>
auto CallWithContext(F&& functor, T* context, Args&&... args)
{
  return functor(context, std::forward<Args>(args)...);
}

// Set ups one-way (write-only) binding between a control and a value reference
template <typename T, typename Control>
static void BindControlToValue(Control* control, T& value)
{
  using control_trait = BindableControlTrait<Control>;
  QObject::connect(control, control_trait::SignalFunc,
                   [&value](T new_value) { value = new_value; });
  CallWithContext(control_trait::SetFunc, control, value);
}

// Set ups one-way (write-only) binding between a control and a value reference, with transformers
template <typename T, typename Control, typename Transform, typename ReverseTransform>
static void BindControlToValue(Control* control, T& value, Transform transform,
                               ReverseTransform reverse_transform)
{
  using control_trait = BindableControlTrait<Control>;
  using control_value =
      typename MemberFunctionArgType<decltype(control_trait::SignalFunc)>::ArgType;

  CallWithContext(control_trait::SetFunc, control, transform(value));

  auto* reverse_transform_function = new std::function<T(control_value)>(reverse_transform);
  QObject::connect(control, &QObject::destroyed, [=]() { delete reverse_transform_function; });
  QObject::connect(control, control_trait::SignalFunc,
                   [&value, reverse_transform_function](T new_value) {
                     value = (*reverse_transform_function)(new_value);
                   });
}
