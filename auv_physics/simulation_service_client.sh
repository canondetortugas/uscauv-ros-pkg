#!/bin/bash

# rosservice call /physics_simulator/simulation_cmd "{command: {type: 1, initial_pose: {position: {x: 0, y: 0, z: 0}, orientation: {x: .707, y: 0, z: 0, w: .707} }, initial_velocity: {linear: {x: 0.05, y: 0, z: 0}, angular: {x: 0, y: 0.1, z: 0} } } }"

rosservice call /physics_simulator/simulation_cmd "{command: {type: 2, initial_pose: {position: {x: 0, y: 0, z: 0}, orientation: {x: 0, y: 0.0 , z: 0, w: 1.0} }, initial_velocity: {linear: {x: 0.0, y: 0, z: 0}, angular: {x: 0, y: 0, z: 0.0} } } }"
