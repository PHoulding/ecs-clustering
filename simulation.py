#!/usr/bin/env python3

#
#   Note this script is not designed to be run from this directory this is here so that it can be shared with the rest
#   of the lab group as an example. This should be run from outside of the ns3 folder, currently in an example folder
#
#  /
#      ns-3-allinone/
#          .....
#      experiment/
#          simulation.py
#
#   This script should be run from the experiment directory
#

import sem
import os
import time

num_runs = 30
experimentName = os.environ.get('NS3_EXPERIMENT', 'ecs_clustering_v1')
ns_path = os.environ.get('NS3_ROOT', '../ns-allinone-3.34/ns-3.34')
script = os.environ.get('NS3_SCRIPT', 'ecs-clustering-example')
results_path = os.environ.get('RESULTS_DIR', os.getcwd())
optimized = os.environ.get('BUILD_PROFILE', 'optimized') == 'optimized'
numThreads = int(os.environ.get('NUM_THREADS', 0))

param_combination = {
    'runTime': [600],
    'totalNodes': [250, 500, 750, 1000],
    'hops': [1],
    'areaWidth': [2000],
    'areaLength': [2000],
    'travellerVelocity': [2.0, 5.0, 10.0, 15.0, 18.0]
}

def runSimulation():
    totalSims = len(sem.manager.list_param_combinations(param_combination)) * num_runs
    toRun = len(campaign.get_missing_simulations(sem.manager.list_param_combinations(param_combination), runs=num_runs))

    print("Running simulations {toRun} of {totalSims} to be run")
    campaign.run_missing_simulations(param_combination, runs=num_runs, stop_on_errors = False)


if __name__ == "__main__":
    campaign_dir = os.path.join(results_path, experimentName)
    figure_dir = os.path.join(results_path, f'{experimentName}_figures')

    if not os.path.exists(figure_dir):
        os.makedirs(figure_dir)
    
    campaign = sem.CampaignManager.new(ns_path, script, campaign_dir, check_repo = False, optimized = optimized, max_parallel_processes = numThreads)

    # generate_values & plots