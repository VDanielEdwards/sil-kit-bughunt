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
#include <stdint.h>
#include <limits.h>
#include "silkit/capi/SilKitMacros.h"
#include "silkit/capi/Types.h"


#pragma pack(push)
#pragma pack(8)

SILKIT_BEGIN_DECLS



typedef uint64_t SilKit_NanosecondsTime; //!< Simulation time

typedef uint64_t SilKit_NanosecondsWallclockTime; //!< Wall clock time since epoch

/*! \brief Join the SIL Kit simulation hosted by the registry listening at URI as a participant.
*
* Join the SIL Kit simulation and become a participant
* based on the given configuration options.
*
* \param outParticipant The pointer through which the simulation participant will be returned (out parameter).
* \param config Configuration of the participant passed as YAML/JSON string
* \param participantName Name of the participant
* \param cRegistryUri The `silkit://` URI of the registry
*
*/
SilKitAPI SilKit_ReturnCode SilKit_Participant_Create(SilKit_Participant** outParticipant, 
    const char* cJsonConfig, const char* cParticipantName, const char* cRegistryUri, SilKit_Bool isSynchronized);

typedef SilKit_ReturnCode (*SilKit_Participant_Create_t)(SilKit_Participant** outParticipant, 
    const char* cJsonConfig, const char* cParticipantName, const char* cRegistryUri, SilKit_Bool isSynchronized);
    
/*! \brief Destroy a simulation participant and its associated simulation elements.
*
* Destroys the simulation participant and its created simulation elements such as e.g. Can controllers.
*
* \param participant The simulation participant to be destroyed.
*
*/
SilKitAPI SilKit_ReturnCode SilKit_Participant_Destroy(SilKit_Participant* participant);

typedef SilKit_ReturnCode (*SilKit_Participant_Destroy_t)(SilKit_Participant* participant);

SILKIT_END_DECLS

#pragma pack(pop)
