//  Copyright (c) 2015 University of Oregon
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <iostream>
#include "apex_api.hpp"

int main (int argc, char *argv[]) {
    int i;
    int final_coalesced_parcels = -1;
    apex::init("coalesce trigger test");

    for (i = 0 ; i < 10 ; i++) {
        apex::sample_value("time per transaction", (1.0 - (i*0.05)));
        sleep(1);
        apex::custom_event(APEX_CUSTOM_EVENT_1, (void *)(&final_coalesced_parcels));
    }

    std::cerr << std::endl;
    std::cerr << "Final coalesced_parcels: " << final_coalesced_parcels << std::endl;
    std::cerr << std::endl;

    apex::finalize();
}

