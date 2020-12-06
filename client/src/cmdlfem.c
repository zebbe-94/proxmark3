//-----------------------------------------------------------------------------
// Copyright (C) 2020 iceman <iceman at icesql.net>
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Low frequency EM4x commands
//-----------------------------------------------------------------------------

#include "cmdlfem.h"
#include "cmdlfem410x.h"
#include "cmdlfem4x05.h"
#include "cmdlfem4x50.h"
#include <inttypes.h>
#include <stdlib.h>
#include "cmdparser.h"     // command_t
#include "comms.h"         // clearCommandBuffer
#include "cmdlf.h"

static int CmdHelp(const char *Cmd);

static command_t CommandTable[] = {
    {"help",  CmdHelp,      AlwaysAvailable, "This help"},
    {"410x",  CmdLFEM410X,  AlwaysAvailable, "EM 4102 commands..."},
    {"4x05",  CmdLFEM4X05,  AlwaysAvailable, "EM 4205 / 4305 / 4369 / 4469 commands..."},
    {"4x50",  CmdLFEM4X50,  AlwaysAvailable, "EM 4350 / 4450 commands..."},
    {NULL, NULL, NULL, NULL}
};

static int CmdHelp(const char *Cmd) {
    (void)Cmd; // Cmd is not used so far
    CmdsHelp(CommandTable);
    return PM3_SUCCESS;
}

int CmdLFEM(const char *Cmd) {
    clearCommandBuffer();
    return CmdsParse(CommandTable, Cmd);
}
