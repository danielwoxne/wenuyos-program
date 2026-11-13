// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/ProjectGuard.hpp"
#include <cassert>
#include <iostream>

using namespace wenuystudio;
int main() {
 ProjectGuard gate;
 const ProjectAction newProject{ProjectActionKind::New,{}};
 const ProjectAction openFile{ProjectActionKind::Open,"Demo.wstudio"};
 const ProjectAction closeWindow{ProjectActionKind::Close,{}};

 // An unchanged project may proceed, but recording always blocks it.
 assert(gate.request(newProject,false,false)==GuardDecision::Proceed);
 assert(gate.request(closeWindow,false,true)==GuardDecision::Busy);
 assert(!gate.hasPending());

 // Cancel closes the prompt; nothing can replace or close the project.
 assert(gate.request(newProject,true,false)==GuardDecision::Prompt);
 assert(gate.request(closeWindow,true,false)==GuardDecision::Busy);
 gate.cancel();
 assert(!gate.hasPending());
 assert(!gate.discard());

 // Discard hands out the originally selected operation exactly once.
 assert(gate.request(openFile,true,false)==GuardDecision::Prompt);
 auto op=gate.discard();
 assert(op && op->kind==ProjectActionKind::Open && op->path=="Demo.wstudio");
 assert(!gate.discard());

 // Saving is asynchronous: neither cancel nor failure may continue.
 assert(gate.request(closeWindow,true,false)==GuardDecision::Prompt);
 assert(gate.beginSave());
 assert(!gate.beginSave());
 assert(gate.request(openFile,true,false)==GuardDecision::Busy);
 assert(!gate.discard());
 assert(!gate.finishedSave(false));
 assert(!gate.hasPending() && !gate.saving());

 // A save completed successfully releases the pending action.
 assert(gate.request(openFile,true,false)==GuardDecision::Prompt);
 assert(gate.beginSave());
 op=gate.finishedSave(true);
 assert(op && op->kind==ProjectActionKind::Open && op->path=="Demo.wstudio");
 assert(!gate.finishedSave(true));

 // Canceling the save chooser must not leave a stale close/new request.
 assert(gate.request(newProject,true,false)==GuardDecision::Prompt);
 assert(gate.beginSave());
 assert(!gate.finishedSave(false));
 assert(gate.request(closeWindow,true,false)==GuardDecision::Prompt);
 gate.cancel();
 assert(!gate.hasPending());

 std::cout<<"PASS: safe project switch, close, discard, save/cancel/error\n";
}
