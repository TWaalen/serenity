/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2020-2021, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ScopeGuard.h>
#include <LibJS/AST.h>
#include <LibJS/Interpreter.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/ECMAScriptFunctionObject.h>
#include <LibJS/Runtime/FunctionEnvironment.h>
#include <LibJS/Runtime/GlobalEnvironment.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Reference.h>
#include <LibJS/Runtime/Shape.h>
#include <LibJS/Runtime/Value.h>

namespace JS {

NonnullOwnPtr<Interpreter> Interpreter::create_with_existing_realm(Realm& realm)
{
    auto& global_object = realm.global_object();
    DeferGC defer_gc(global_object.heap());
    auto interpreter = adopt_own(*new Interpreter(global_object.vm()));
    interpreter->m_global_object = make_handle(&global_object);
    interpreter->m_realm = make_handle(&realm);
    return interpreter;
}

Interpreter::Interpreter(VM& vm)
    : m_vm(vm)
{
}

Interpreter::~Interpreter()
{
}

ThrowCompletionOr<Value> Interpreter::run(GlobalObject& global_object, const Program& program)
{
    // FIXME: Why does this receive a GlobalObject? Interpreter has one already, and this might not be in sync with the Realm's GlobalObject.

    auto& vm = this->vm();
    VERIFY(!vm.exception());

    VM::InterpreterExecutionScope scope(*this);

    ExecutionContext execution_context(heap());
    execution_context.current_node = &program;
    execution_context.this_value = &global_object;
    static FlyString global_execution_context_name = "(global execution context)";
    execution_context.function_name = global_execution_context_name;
    execution_context.lexical_environment = &realm().global_environment();
    execution_context.variable_environment = &realm().global_environment();
    execution_context.realm = &realm();
    execution_context.is_strict_mode = program.is_strict_mode();
    MUST(vm.push_execution_context(execution_context, global_object));
    auto completion = program.execute(*this, global_object);

    // At this point we may have already run any queued promise jobs via on_call_stack_emptied,
    // in which case this is a no-op.
    vm.run_queued_promise_jobs();

    vm.run_queued_finalization_registry_cleanup_jobs();

    vm.pop_execution_context();

    vm.finish_execution_generation();

    if (completion.is_abrupt()) {
        VERIFY(completion.type() == Completion::Type::Throw);
        return completion.release_error();
    }
    return completion.value().value_or(js_undefined());
}

GlobalObject& Interpreter::global_object()
{
    return static_cast<GlobalObject&>(*m_global_object.cell());
}

const GlobalObject& Interpreter::global_object() const
{
    return static_cast<const GlobalObject&>(*m_global_object.cell());
}

Realm& Interpreter::realm()
{
    return static_cast<Realm&>(*m_realm.cell());
}

const Realm& Interpreter::realm() const
{
    return static_cast<const Realm&>(*m_realm.cell());
}

}
