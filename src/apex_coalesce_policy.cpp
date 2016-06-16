//  APEX OpenMP Policy
//
//  Copyright (c) 2015 University of Oregon
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <memory>
#include <set>
#include <utility>
#include <cstdlib>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <string>
#include <stdio.h>
#include <algorithm> 
#include <functional>
#include <cctype>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
            
#include <omp.h>

#include "apex_api.hpp"
#include "apex_policies.hpp"


static apex_event_type trigger;
static int apex_coalesce_policy_tuning_window = 3;
static apex_ah_tuning_strategy apex_coalesce_policy_tuning_strategy = apex_ah_tuning_strategy::NELDER_MEAD;
static std::unordered_map<std::string, std::shared_ptr<apex_tuning_request>> * apex_coalesce_policy_tuning_requests;
static bool apex_coalesce_policy_verbose = false;
static bool apex_coalesce_policy_use_history = false;
static bool apex_coalesce_policy_running = false;
static std::string apex_coalesce_policy_history_file = "";

static const std::list<std::string> default_coalesce_space{"2", "4", "8", "16", "24", "32", "64", "128", "256", "512", "1024"};

static const std::list<std::string> * coalesce_space = nullptr;

static apex_policy_handle * custom_event_policy;

static int get_hpx_params(std::shared_ptr<apex_tuning_request> request) {
    std::shared_ptr<apex_param_long> parcels_param = std::static_pointer_cast<apex_param_long>(request->get_param("coalesced_parcels"));
    const int coalesced_parcels = (int)(parcels_param->get_value());
    if(apex_coalesce_policy_verbose) {
        const std::string & name = request->get_name();
        fprintf(stderr, "name: %s, coalesced parcels: %d\n", name.c_str(), coalesced_parcels);
    }
    return coalesced_parcels;
}


void handle_event(const std::string & name, const apex_context &context) {
    static apex_tuning_session_handle session;
    int * tmp = (int*)context.data;
    auto search = apex_coalesce_policy_tuning_requests->find(name);
    if(search == apex_coalesce_policy_tuning_requests->end()) {
        // Start a new tuning session.
        if(apex_coalesce_policy_verbose) {
            fprintf(stderr, "Starting tuning session for %s\n", name.c_str());
        }
        std::shared_ptr<apex_tuning_request> request{std::make_shared<apex_tuning_request>(name)};
        apex_coalesce_policy_tuning_requests->insert(std::make_pair(name, request));

        // Create an event to trigger this tuning session.
        //trigger = apex::register_custom_event(name);
        request->set_trigger(APEX_CUSTOM_EVENT_1); // something bogus?

        // Create a metric
        std::function<double(void)> metric = [=]()->double{
            apex_profile * profile = apex::get_profile(name);
            if(profile == nullptr) {
                //std::cerr << "ERROR: no profile for " << name << std::endl;
                return 0.0;
            } 
            if(profile->calls == 0.0) {
                //std::cerr << "ERROR: calls = 0 for " << name << std::endl;
                return 0.0;
            }
            double result = profile->accumulated/profile->calls;
            if(apex_coalesce_policy_verbose) {
                fprintf(stderr, "time per call: %f\n", result);
            }
            return result;
        };
        request->set_metric(metric);

        // Set apex_coalesce_policy_tuning_strategy
        request->set_strategy(apex_coalesce_policy_tuning_strategy);

        // Create a parameter for number of threads.
        // std::shared_ptr<apex_param_enum> threads_param = request->add_param_enum("coalesced_parcels", "16", *coalesce_space);
        std::shared_ptr<apex_param_long> threads_param = request->add_param_long("coalesced_parcels", 256, 4, 2048, 4);

        // Set HPX runtime parameters to initial values.
        *tmp = get_hpx_params(request);

        // Start the tuning session.
        session = apex::setup_custom_tuning(*request);
    } else {
        // We've seen this region before.
        //apex_custom_tuning_policy(session, context);
        std::shared_ptr<apex_tuning_request> request = search->second;
        *tmp = get_hpx_params(request);
    }
}

int policy(const apex_context context) {
    if(context.data != nullptr) {
        handle_event("time per transaction", context);
    }
    return APEX_NOERROR;
}

void print_summary() {
    std::cout << std::endl << "HPX final settings: " << std::endl;
    for(auto request_pair : *apex_coalesce_policy_tuning_requests) {
        auto request = request_pair.second;
        const std::string & name = request->get_name();
        std::shared_ptr<apex_param_long> parcels_param = std::static_pointer_cast<apex_param_long>(request->get_param("coalesced_parcels"));
        const int coalesced_parcels = (int)(parcels_param->get_value());
        //const std::string & cp = std::static_pointer_cast<apex_param_long>(request->get_param("coalesced_parcels"))->get_value();
        const std::string converged = request->has_converged() ? "CONVERGED" : "NOT CONVERGED";
        std::cout << "name: " << name << ", coalesced_parcels: " << coalesced_parcels << " " << converged << std::endl;
    }
    std::cout << std::endl;
}

bool parse_space_file(const std::string & filename) {
    using namespace rapidjson;
    std::ifstream space_file(filename, std::ifstream::in);
    if(!space_file.good()) {
        std::cerr << "Unable to open parameter space specification file " << filename << std::endl;
        assert(false);
        return false;
    } else {
        IStreamWrapper space_file_wrapper(space_file);
        Document document;
        document.ParseStream(space_file_wrapper);
        if(!document.IsObject()) {
            std::cerr << "Parameter space file root must be an object." << std::endl;
            return false;
        }
        if(!document.HasMember("tuning_space")) {
            std::cerr << "Parameter space file root must contain a member named 'tuning_space'." << std::endl;
            return false;
        }

        const auto & tuning_spec = document["tuning_space"];
        if(!tuning_spec.IsObject()) {
            std::cerr << "Parameter space file's 'tuning_space' member must be an object." << std::endl;
            return false;
        }
        if(!tuning_spec.HasMember("coalesced_parcels")) {
            std::cerr << "Parameter space file's 'tuning_space' object must contain a member named 'coalesced_parcels'" << std::endl;
            return false;
        }

        const auto & coalesced_parcels_array = tuning_spec["coalesced_parcels"];

        // Validate array types
        if(!coalesced_parcels_array.IsArray()) {
            std::cerr << "Parameter space file's 'coalesced_parcels' member must be an array." << std::endl;
            return false;
        }

        // coalesced_parcels
        std::list<std::string> coalesced_list;
        for(auto itr = coalesced_parcels_array.Begin(); itr != coalesced_parcels_array.End(); ++itr) {
              if(itr->IsInt()) {
                  const int this_coalesced = itr->GetInt();
                  const std::string this_coalesced_str = std::to_string(this_coalesced);
                  coalesced_list.push_back(this_coalesced_str);
              } else if(itr->IsString()) {
                  const char * this_coalesced = itr->GetString();
                  const std::string this_coalesced_str = std::string(this_coalesced, itr->GetStringLength());
                  coalesced_list.push_back(this_coalesced_str);
              } else {
                  std::cerr << "Parameter space file's 'coalesced_parcels' member must contain only integers or strings" << std::endl;
                  return false;
              }
        }
        coalesce_space = new std::list<std::string>{coalesced_list};

    }
    return true;
}

void print_tuning_space() {
    std::cerr << "Tuning space: " << std::endl;
    std::cerr << "\tcoalesced_parcels: ";
    if(coalesce_space == nullptr) {
        std::cerr << "NULL";
    } else {
        for(auto coalesced : *coalesce_space) {
            std::cerr << coalesced << " ";
        }
    }
    std::cerr << std::endl;
}


int register_policy() {
    // Process environment variables
    
    // APEX_COALESCE_VERBOSE
    const char * apex_coalesce_policy_verbose_option = std::getenv("APEX_COALESCE_VERBOSE");
    if(apex_coalesce_policy_verbose_option != nullptr) {
        apex_coalesce_policy_verbose = 1;
    }

    // APEX_COALESCE_WINDOW
    const char * option = std::getenv("APEX_COALESCE_WINDOW");
    if(option != nullptr) {
        apex_coalesce_policy_tuning_window = atoi(option);        
    }

    if(apex_coalesce_policy_verbose) {
        std::cerr << "apex_coalesce_policy_tuning_window = " 
                  << apex_coalesce_policy_tuning_window << std::endl;
    }

    // APEX_COALESCE_STRATEGY
    const char * apex_coalesce_policy_tuning_strategy_option = std::getenv("APEX_COALESCE_STRATEGY");
    std::string apex_coalesce_policy_tuning_strategy_str = (apex_coalesce_policy_tuning_strategy_option == nullptr) ? std::string() : std::string(apex_coalesce_policy_tuning_strategy_option);
    std::transform(apex_coalesce_policy_tuning_strategy_str.begin(), apex_coalesce_policy_tuning_strategy_str.end(), apex_coalesce_policy_tuning_strategy_str.begin(), std::ptr_fun<int, int>(std::toupper));
    if(apex_coalesce_policy_tuning_strategy_str.empty()) {
        // default
        apex_coalesce_policy_tuning_strategy = apex_ah_tuning_strategy::NELDER_MEAD;
        std::cerr << "Using default tuning strategy (NELDER_MEAD)" << std::endl;
    } else if(apex_coalesce_policy_tuning_strategy_str == "EXHAUSTIVE") {
        apex_coalesce_policy_tuning_strategy = apex_ah_tuning_strategy::EXHAUSTIVE;
        std::cerr << "Using EXHAUSTIVE tuning strategy." << std::endl;
    } else if(apex_coalesce_policy_tuning_strategy_str == "RANDOM") {
        apex_coalesce_policy_tuning_strategy = apex_ah_tuning_strategy::RANDOM;
        std::cerr << "Using RANDOM tuning strategy." << std::endl;
    } else if(apex_coalesce_policy_tuning_strategy_str == "NELDER_MEAD") {
        apex_coalesce_policy_tuning_strategy = apex_ah_tuning_strategy::NELDER_MEAD;
        std::cerr << "Using NELDER_MEAD tuning strategy." << std::endl;
    } else if(apex_coalesce_policy_tuning_strategy_str == "PARALLEL_RANK_ORDER") {
        apex_coalesce_policy_tuning_strategy = apex_ah_tuning_strategy::PARALLEL_RANK_ORDER;
        std::cerr << "Using PARALLEL_RANK_ORDER tuning strategy." << std::endl;
    } else {
        std::cerr << "Invalid setting for APEX_COALESCE_STRATEGY: " << apex_coalesce_policy_tuning_strategy_str << std::endl;
        std::cerr << "Will use default of NELDER_MEAD." << std::endl;
        apex_coalesce_policy_tuning_strategy = apex_ah_tuning_strategy::NELDER_MEAD;
    }

    // APEX_COALESCE_HISTORY
    const char * apex_coalesce_policy_history_file_option = std::getenv("APEX_COALESCE_HISTORY");
    if(apex_coalesce_policy_history_file_option != nullptr) {
        apex_coalesce_policy_history_file = std::string(apex_coalesce_policy_history_file_option);
        if(!apex_coalesce_policy_history_file.empty()) {
            apex_coalesce_policy_use_history = true;
        }
    }

    // APEX_COALESCE_SPACE
    const char * apex_coalesce_policy_space_file_option = std::getenv("APEX_COALESCE_SPACE");
    bool using_space_file = false;
    if(apex_coalesce_policy_space_file_option != nullptr) {
        const std::string apex_opemp_policy_space_file{apex_coalesce_policy_space_file_option};
        using_space_file = parse_space_file(apex_opemp_policy_space_file);
        if(!using_space_file) {
            std::cerr << "WARNING: Unable to use tuning space file " << apex_coalesce_policy_space_file_option << ". Using default tuning space instead." << std::endl;
        }
    } 

    // Set up the search spaces
    if(!using_space_file) {
        if(apex_coalesce_policy_verbose) {
            std::cerr << "Using default tuning space." << std::endl;
        }
        coalesce_space   = &default_coalesce_space;
    } else {
        if(apex_coalesce_policy_verbose) {
            std::cerr << "Using tuning space from " << apex_coalesce_policy_space_file_option << std::endl;
        }
    }

    if(apex_coalesce_policy_verbose) {
        print_tuning_space();
    }


    // Register the policy functions with APEX
    std::function<int(apex_context const&)> policy_fn{policy};
    custom_event_policy = apex::register_policy(APEX_CUSTOM_EVENT_1, policy_fn);    
    if(custom_event_policy == nullptr) {
        return APEX_ERROR;
    } else {
        return APEX_NOERROR;
    }
}
 
extern "C" {

    int apex_plugin_init() {
        if(!apex_coalesce_policy_running) {
            fprintf(stderr, "apex_coalesce_policy init\n");
            apex_coalesce_policy_tuning_requests = new std::unordered_map<std::string, std::shared_ptr<apex_tuning_request>>(); 
            int status =  register_policy();
            apex_coalesce_policy_running = true;
            return status;
        } else {
            fprintf(stderr, "Unable to start apex_coalesce_policy because it is already running.\n");
            return APEX_ERROR;
        }
    }

    int apex_plugin_finalize() {
        if(apex_coalesce_policy_running) {
            fprintf(stderr, "apex_coalesce_policy finalize\n");
            //apex::deregister_policy(custom_event_policy);
            print_summary();
            delete apex_coalesce_policy_tuning_requests;
            apex_coalesce_policy_running = false;
            return APEX_NOERROR;
        } else {
            fprintf(stderr, "Unable to stop apex_coalesce_policy because it is not running.\n");
            return APEX_ERROR;
        }
    }

}
