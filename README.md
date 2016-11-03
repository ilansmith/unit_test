# utest core

A framework by [Ilan Smith](https://github.com/ilansmith) for unit-testing.

## Introduction
The utest (pronounced *"U-test"*) unit test suite provides a user space isolated environment for code testing and facilitates rigorous scrutinizing at function level resolution.

The solution works best for features which reside in their own git repository - outside of the development tree - at least during their development phase. In such cases it is easy to import the feature repository into utest and interface with it just as the main code would do.

Every feature repository tested by utest is expected to consist of at least:
* Its implementation in code
* A Ubuild file where its build policy is defined
* An additional \*.c file with test implementations for the feature

## Design

### Design Goals
* *The framework is to run entirely in user space*<br>
  Since unit-tests are intended to verify functionality and algorithm
  correctness - the advantage of testing in user space goes without saying
* *Minimal - and preferably - no changes in tested code due to utest*<br>
  Indeed, if you examine the tested features in **ut** you will find no evidence of any testing framework in their source code. In other words, the code under test is oblivious to whether it's running in its original or in utest realm.
* *Writing tests must be easy*<br>
  The robust mock function support in conjunction with a wide set of unit-test
  utility functions make it easy to easily write tests with vast coverage,
  including both success and failure oriented testing and edge conditions

<p align="center">
  <img src="https://raw.githubusercontent.com/k-unit/core/master/images/kunit_component_layout.png" width="1000"/>
</p>

### Break Down

* The project is comprised of multiple git repositories, of which this is one - namely, the [utest core](https://github.com/ilansmith/core "A framework for unit-testing").
* The [env](https://github.com/ilansmith/env.git "Environment setup for various repo projects") repository completes the project setup after download by
  * Verifying prerequisite software is installed
  * The utest build is developed in **env/build** and mimics the Linux kernel's recursive build
* **ut** is where the imported features reside. It itself is not a git repository but rather hosts multiple features which are

### Test Flow
Following is how utest conducts its tests.<br>
Each tested feature (git repository under **ut**) includes a *test package* of numerous *tests* to be run by utest:

<p align="left">
  <img src="https://raw.githubusercontent.com/k-unit/core/master/images/kunit_test_flow.png" width="500"/>
</p>

## Setup and Build
As mentioned above, this is not a stand alone project and as such it is not intended to be cloned directly but rather as part of a larger multi-git project. Please use the [project](https://github.com/ilansmith/project.git "utest Multi-Git Setup Repository") repository to set utest up!<br>
<br>
To setup the project we use the [Android *repo* tool](https://storage.googleapis.com/git-repo-downloads/repo "Android repo Tool"). See the [project](https://github.com/ilansmith/project.git "Multi-Git Setup Repository") repository for information on getting *repo*. 

### Setup Stages:
 1. Create your *utest project root directory* and from within it issue:<br>
    `repo init -u https://github.com/ilansmith/project -m utest.xml`<br>
 2. Update the project by issuing:<br>
    `repo sync`
 3. By default the versions checked out as defined in the repo manifest don't have a name and are called *no_branch*.<br>
    It is good practice (though not mandatory) to set a branch name in all repositories in the project by issuing:<br>
    `repo start <branch_name> --all`<br>
    Where *branch_name* is to be the name of the local branch on each of the project's git repositories.
 4. Finalize the project setup by running `./env/setup.sh` from project root. This completes project environment setup and you can now build and run it.<br>
    Use `./env/setup.sh --help` to see the script's usage options.<br>

Use `make --help` in the project root to see the available build options.

## Usage
<br>
The following options are supported by *utest*:

* **-l, --list [name1 name2 ...]**<br>
  <br>
  List available unit tests<br>
  <br>
  If no test names are specified then a summary of all available tests is listed.<br>
  Example:<br>
  
  `./utest --list`
  
  | num  | module      | description                                |
  |:----:|:----------- |:------------------------------------------ |
  |  1.  | slw         | Sliding Window                             |
  |  2.  | sdp         | SMPTE ST-2110 Session Description Protocol |
  |  3.  | vector      | CPP std::Vector implementation in C        |
  
  Specific test names may be used in which case they will be listed in more detail. The available test names are those listed when using --list without arguments.<br>
  Example:<br>
  
  `./utest --list slw ffu`
  
  **Sliding Window Unit Tests**<br>
  1\. Simple test (unit test demo)<br>
  2\. Basic test: 4 R/W bursts<br>
  3\. Basic test: 6 R/W bursts<br>
  4\. R/W bursts combined with NONE bursts<br>
  5\. Intensive combined test<br>
  6\. Resize test<br>
  **Field Firmware Upgrade Unit Tests**<br>
  1\. ffu map sg - no chaining<br>
  2\. ffu map sg - multi chaining<br>
  3\. area init test<br>
  4\. ffu write test<br>

* **-t, --test \<name\> [#1 [#2 [...]]]**<br>
  <br>
  Specify a specific unit test to run<br>
  <br>
  *name* is given by using the *--list* option without arguments. If no test ids are provided, then all the tests of name are run.<br>

  Execution can be limited to running a subset of name's tests. This is done by specific test numbers as are given by --list name.<br>
  Example:<br>
  
  <table>
  <tr><td>./utest</td> <td>all tests will run</td></tr>
  <tr><td>./utest dev_tree</td> <td>all dev_tree unit tests will run</td></tr>
  <tr><td>./utest ffu 2 4</td> <td>ffu tests #2 and #4 will run</td></tr>
  </table>

## Adding New Tests
Adding new tests to an existing *test package* is designed to be simple and straight forward.<br>

The simplicity of adding new tests depends on there being sufficient mock and utility function support within utest. If there is lacking mock or utility support - it will need to be added (and tested) before you can use it in tests. This can vary from being quite trivial to fairly complex.

Adding a new *test package* to utest requires:
* Importing feature and placing it in **ut**
* Implementing a set of tests for the utest framework<br>
  The feature's test object-file must be added to the *obj-ut* target in the feature's *Ubuild* file
* In the [env](https://github.com/ilansmith/env.git "Environment setup and build") repository
  * Add any required `CONFIG=` entries to *build/utest_defconfig*. This file
    gets copied to utest root directory and renamed to *.config* at during setup
  * Enable/Disable the building in of the feature in *build/Ubuild*. This file
    gets copied to *ut/Ubuild* during setup
* In the [core](https://github.com/ilansmith/core "A framework for unit-testing") repository (this one)
  * Link each feature to the core for testing in *tests.h*

