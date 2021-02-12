# Linktest
Point-to-point link test for running on FlockLab 2 (measures packet reception ratio, RSSI, and CRC errors).

## Initial Checkout and Compile of the Project
* See [instructions](https://gitlab.ethz.ch/tec/research/dpp/wiki/-/wikis/flora/home#clone-compile-run) for flora apps

## Running a Test
1. Set the `TESTCONFIG_xxx` and `RADIOCONFIG_xxx` configuration parameters. (in `app_config.h`)  
2. Build the project.  
3. Run `./Scripts/run_linktest.py`  
   (requires the  [`flocklab-tools`](https://pypi.org/project/flocklab-tools/) and `GitPython` python package)

## Evaluation of a Test
1. Run eval script: `./Scripts/eval_linktest.py [testno]`  
   (Results are available as generated `.html` and `.pkl` files in `./data/`.)
