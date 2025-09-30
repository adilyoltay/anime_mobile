# State Machine Implementation Plan

## Phase 1: Core Structure (Step 1-2)

### TypeKeys
- **StateMachine** (53) - Container
- **StateMachineLayer** (57) - Layer container
- **EntryState** (63) - Entry point
- **AnimationState** (61) - Plays animation
- **ExitState** (62) - Exit point
- **AnyState** (60) - Catch-all state

### Property Keys
- StateMachine: name (4) only
- StateMachineLayer: name (4) only
- AnimationState: animationId (149) - reference to animation

## Phase 2: Inputs (Step 3)

### Input Types
- **StateMachineBool** (59) - Boolean input
  - value (141): bool
- **StateMachineNumber** (56) - Number input
  - value (142): double
- **StateMachineTrigger** (58) - One-shot trigger
  - No value property

## Phase 3: Transitions (Step 4)

### StateTransition (65)
- stateToId (151): uint - Target state ID
- flags (152): uint - Transition flags
- duration (158): uint - Transition duration (ms)
- exitTime (160): uint - Exit time (0-100%)

## Phase 4: Conditions (Step 5)

### TransitionBoolCondition (67)
- inputId (155): uint - Reference to bool input
- value (157): bool - Expected value

### TransitionNumberCondition (68)
- inputId (155): uint
- value (156): double
- op (153): uint - Operator (0==, 1!=, 2<, 3<=, 4>, 5>=)

---

## Implementation Steps

### ✅ Step 1: Minimal StateMachine
- Add StateMachine object
- Add 1 bool input
- **Demo:** state_machine_minimal.riv (StateMachine exists, no states)

### Step 2: Add States
- EntryState
- 2 AnimationStates
- **Demo:** state_machine_states.riv (States exist, no transitions)

### Step 3: Add Transitions
- EntryState → IdleState
- IdleState ↔ HoverState (with bool condition)
- **Demo:** state_machine_hover.riv (Interactive hover button)

### Step 4: Multiple Inputs
- Bool + Number inputs
- Transitions with number conditions
- **Demo:** state_machine_slider.riv (Slider with value threshold)

### Step 5: Complex State Machine
- Multiple layers
- AnyState usage
- Multiple conditions
- **Demo:** state_machine_complete.riv (Full featured)

---

## Current Status: Starting Step 1
