// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/RcPtr.h"

static int count = 0;

struct countingObject
{
  countingObject() { count++; }
  ~countingObject() { count--; }
};


TEST(RcPtr, Counts) {
    {
        auto ptr1 = Common::make_rc<countingObject>();
        {

            EXPECT_EQ(1, ptr1.use_count());
            EXPECT_EQ(1, count);

            auto ptr2 = ptr1;

            Common::rc_ptr<countingObject> ptr3;

            EXPECT_EQ(0, ptr3.use_count());

            ptr3 = ptr1;

            EXPECT_EQ(3, ptr1.use_count());
            EXPECT_EQ(1, count);

            ptr2.reset();

            EXPECT_EQ(2, ptr1.use_count());
            EXPECT_EQ(1, count);

            ptr3 = Common::rc_ptr<countingObject>::make();

            EXPECT_EQ(1, ptr3.use_count());
            EXPECT_EQ(1, ptr1.use_count());
            EXPECT_EQ(2, count);

            ptr2 = ptr3;

            EXPECT_EQ(2, ptr3.use_count());
            EXPECT_EQ(1, ptr1.use_count());
            EXPECT_EQ(2, count);

            ptr1 = std::move(ptr2);

            EXPECT_EQ(nullptr, ptr2.get());

            EXPECT_EQ(2, ptr3.use_count());
            EXPECT_EQ(2, ptr1.use_count());
            EXPECT_EQ(1, count);
        }

        EXPECT_EQ(1, ptr1.use_count());
        EXPECT_EQ(1, count);
    }
    EXPECT_EQ(0, count);
}