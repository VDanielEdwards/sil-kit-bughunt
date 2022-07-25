/* Copyright (c) 2022 Vector Informatik GmbH

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#pragma once
#include <string>

namespace SilKit {
namespace Services {
namespace Orchestration {

class ILifecycleState
{
public:
    virtual ~ILifecycleState() = default;
    virtual void RunSimulation(std::string reason) = 0;
    virtual void PauseSimulation(std::string reason) = 0;
    virtual void ContinueSimulation(std::string reason) = 0;

    virtual void StopNotifyUser(std::string reason) = 0;
    virtual void StopHandlerDone(std::string reason) = 0;

    virtual void Restart(std::string reason) = 0;

    virtual void ShutdownNotifyUser(std::string reason) = 0;
    virtual void ShutdownHandlerDone(std::string reason) = 0;

    virtual void AbortSimulation(std::string reason) = 0;
    virtual void Error(std::string reason) = 0;

    virtual void NewSystemState(SystemState systemState) = 0;

    virtual auto toString() -> std::string = 0;
    virtual auto GetParticipantState() -> ParticipantState = 0;
};

} // namespace Orchestration
} // namespace Services
} // namespace SilKit
