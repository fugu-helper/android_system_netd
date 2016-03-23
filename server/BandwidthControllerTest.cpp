/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * BandwidthControllerTest.cpp - unit tests for BandwidthController.cpp
 */

#include <string>
#include <vector>
#include <stdio.h>

#include <gtest/gtest.h>

#include "BandwidthController.h"

std::vector<std::string> gCmds = {};

extern "C" int fake_android_fork_exec(int argc, char* argv[], int *status, bool, bool) {
    std::string cmd = argv[0];
    for (int i = 1; i < argc; i++) {
        cmd += " ";
        cmd += argv[i];
    }
    gCmds.push_back(cmd);
    *status = 0;
    return 0;
}

FILE *fake_popen(const char *, const char *) {
    return NULL;
};

void expectIptablesCommands(std::vector<std::string> expectedCmds) {
    EXPECT_EQ(expectedCmds.size() * 2, gCmds.size());
    if (expectedCmds.size() * 2 != gCmds.size()) return;

    for (size_t i = 0; i < expectedCmds.size(); i ++) {
        EXPECT_EQ("/system/bin/iptables -w " + expectedCmds[i], gCmds[2 * i]);
        EXPECT_EQ("/system/bin/ip6tables -w " + expectedCmds[i], gCmds[2 * i + 1]);
    }

    gCmds.clear();
}

class BandwidthControllerTest : public ::testing::Test {
public:
    BandwidthControllerTest() {
        BandwidthController::execFunction = fake_android_fork_exec;
        BandwidthController::popenFunction = fake_popen;
        gCmds.clear();
    }
    BandwidthController mBw;
};


TEST_F(BandwidthControllerTest, TestEnableBandwidthControl) {
    mBw.enableBandwidthControl(false);
    std::vector<std::string> expected = {
        "-F bw_INPUT",
        "-F bw_OUTPUT",
        "-F bw_FORWARD",
        "-F bw_happy_box",
        "-F bw_penalty_box",
        "-F bw_costly_shared",
        "-t raw -F bw_raw_PREROUTING",
        "-t mangle -F bw_mangle_POSTROUTING",
        "-A bw_INPUT -m owner --socket-exists",
        "-A bw_OUTPUT -m owner --socket-exists",
        "-t raw -A bw_raw_PREROUTING -m owner --socket-exists",
        "-t mangle -A bw_mangle_POSTROUTING -m owner --socket-exists",
        "-A bw_costly_shared --jump bw_penalty_box",
        "-A bw_costly_shared --jump bw_happy_box",
        "-A bw_costly_shared --jump RETURN",
        "-A bw_happy_box -m owner --uid-owner 0-9999 --jump RETURN",
    };
    expectIptablesCommands(expected);
}

TEST_F(BandwidthControllerTest, TestEnableDataSaver) {
    mBw.enableDataSaver(true);
    std::vector<std::string> expected = {
        "-R bw_costly_shared 3 --jump REJECT",
    };
    expectIptablesCommands(expected);

    mBw.enableDataSaver(false);
    expected = {
        "-R bw_costly_shared 3 --jump RETURN",
    };
    expectIptablesCommands(expected);
}