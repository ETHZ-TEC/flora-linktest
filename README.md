## Linktest
Point-to-point link test for running on Flocklab (measures packet reception ratio and RSSI).

Running a test:
3. Set the `TESTCONFIG_xxx` and `RADIOCONFIG_xxx` configuration paramters. (in `app_config.h`)
5. Build the project.
6. Run `./Scripts/run_linktest.py` (requires the  [`flocklab-tools`](https://pypi.org/project/flocklab-tools/) and `GitPython` python package)
