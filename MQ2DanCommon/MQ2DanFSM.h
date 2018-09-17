#pragma once

// pretty simple FSM, trying to reduce vtable calls and do a bunch of error checking at compile time.
// USE:
//	- declare all events that you want to use. These events can have data attached to them because we will be passing instances around
//	- declare a class that inherits the FSM reflexively (this shouldn't be instantiated, we do a lot of work through statics)
//	- declare all state classes, derived from the base class (it's a good idea to forward declare all of them)
//	- declare enter(), exit(), and handle_event(E event) for every event you want to handle in the base class
//	- override those declaration in the state classes where you need
//	- now can use the base class like a singleton, and start the FSM with the starting state as a template parameter
//	- all interaction with the FSM is done by instantiating events and dispatching them through the base class
//
// CANONICAL LIGHT SWITCH EXAMPLE:
//
// class Toggle : public MQ2DanFSM::event {} // this could have data in it if we cared
// class Switch : public MQ2DanFSM::fsm<Switch> { // note the reflexive templating
// public:
//   virtual void enter() {} // we make this virtual so we can do stuff with it in the states
//   void exit() {} // not really going to use this, so just stub it
//   virtual void handle_event(Toggle const & _event) {} // we need one of these for each event we call dispatch for (only one here)
// }
//
// class On;
// class Off;
//
// class On : public Switch {
// public:
//   void enter() override { std::cout << "ON" << std::endl; }
//   void handle_event(Toggle const & _event) override { transition<Off>(); } // make use of the transition helpers here since fsm<> is a base class
// }
//
// class Off : public Switch {
// public:
//   void enter() override { std::cout << "OFF" << std::endl; }
//   void handle_event(Toggle const & _event) override { transition<On>(); } // make use of the transition helpers here since fsm<> is a base class
// }
//
// int main() {
//   Toggle toggle;
//
//   Switch::start<Off>();     // OFF
//   Switch::dispatch(toggle); // ON
//   Switch::dispatch(toggle); // OFF
// }


#include <functional>
#include <type_traits>

namespace MQ2DanFSM {
    // first create a singleton object for our states
    template<typename T> class state {
    public:
        using value_t = T;
        using instance_t = state<T>;
        static T value;
    };

    // Can add data to events because we pass instances around
    class event {}; // base class for all events

    template <typename T>
    typename state<T>::value_t state<T>::value;

    // here is the actual FSM object, which is also a singleton. It manages the current state pointer resource.
    template<typename T>
    class fsm {
    public:
        using fsm_t = fsm<T>;
        using state_ptr_t = T *;
        static state_ptr_t current_state_p;

        // static state instance accessor
        template<typename S, typename std::enable_if<std::is_same<fsm_t, typename S::fsm_t>::value, S>::type* = nullptr>
        static constexpr S& get_state() {
            return state<S>::value;
        }

        // helper function for checking current state
        template<typename S, typename std::enable_if<std::is_same<fsm_t, typename S::fsm_t>::value, S>::type* = nullptr>
        static constexpr bool is_in_state() {
            return current_state_p == &state<S>::value;
        }

        template<typename S>
        static void set_initial_state() {
            current_state_p = &state<S>::value;
        }

        template<typename S>
        static void start() {
            set_initial_state<S>();
            current_state_p->enter();
        }

        // this needs to be templated in order for the template specializations of handle_event to work
        template<class E, typename std::enable_if<std::is_base_of<event, E>::value, E>::type* = nullptr>
        static void dispatch(E const& _event) {
            current_state_p->handle_event(_event); // here is the only vtable lookup -- be sure to declare a virtual handle_event for every event type we need to handle, or this won't compile
        }

    protected:
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Transition functions

        template<typename S>
        void transition() {
            current_state_p->exit();
            current_state_p = &state<S>::value;
            current_state_p->enter();
        }

        template<typename S>
        void transition(std::function<bool()> condition) {
            if (condition()) { transition<S>(); }
        }

        template<typename S>
        void transition(std::function<void()> action) {
            current_state_p->exit();
            action();
            current_state_p = &state<S>::value;
            current_state_p->enter();
        }

        template<typename S>
        void transition(std::function<void()> action, std::function<bool()> condition) {
            if (condition()) { transition<S>(action); }
        }
    };

    template<typename T>
    typename fsm<T>::state_ptr_t fsm<T>::current_state_p;
}
