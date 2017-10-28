# kunit core

A framework by [Ilan Smith](https://github.com/ilansmith) for unit-testing kernel code.

## Introduction
The kunit (pronounced *"kay unit"*) unit test suite provides a user space isolated environment for kernel code testing and facilitates rigorous scrutinizing at function level resolution. The solution is based on a robust set of mocking functions as well as framework utility functions. The mocking functions are trivial(ish) implementations in user space of kernel functions.

The solution works best for kernel features which reside in their own git repository - outside of the kernel tree - at least during their development phase. In such cases it is easy to import the feature repository into kunit and interface with it just as the kernel would do.

Every kernel feature repository tested by kunit is expected to consist of at least:
* Its implementation in code
* A Kbuild file where its build policy is defined
* An optional set of patches intended for injecting code into the kernel which allows it to interface with the feature
* An additional \*.c file with kunit test implementations for the feature

## Design

### Design Goals
* *The framework is to run entirely in user space*<br>
  Since unit-tests are intended to verify functionality and algorithm
  correctness - the advantage of testing in user space goes without saying
* *Minimal - and preferably - no changes in tested code due to kunit*<br>
  Indeed, if you examine the tested features in **ut** you will find no evidence of any testing framework in their kernel source code. In other words, the code under test is oblivious to whether it's running in the kernel or in kunit realm.
* *Writing tests must be easy*<br>
  The robust mock function support in conjunction with a wide set of unit-test
  utility functions make it easy to easily write tests with vast coverage,
  including both success and failure oriented testing and edge conditions
* *Facilitate continual growth of mock and utility support*<br>
  This is done by organizing the code in a kernel tree like layout. So  when adding new mock functions - you put them in an identical location of the original function in the real kernel tree

<p align="center">
  <img src="https://raw.githubusercontent.com/k-unit/core/master/images/kunit_component_layout.png" width="1000"/>
</p>

### Break Down

* The project is comprised of multiple git repositories, of which this is one - namely, the [kunit core](https://github.com/k-unit/core "A framework for unit-testing kernel in user space").
* The [env](https://github.com/k-unit/env.git "Environment setup for various repo projects") repository completes the project setup after download by
  * Verifying prerequisite software is installed
  * Applying feature patches at **kernel/patches** (which is not a git repository) to allow kunit to interface with the features just as the kernel would
  * The kunit build is developed in **env/build** and mimics the kernel's recursive build
* The [kernel/emu](https://github.com/k-unit/emu "Kernel emulation module for kunit") repository contains
  * **lnx**: the set of kernel mock functions
  * **kut**: all required utility functions and a set of tests for verifying correctness of the mock functions
* **ut** is where the imported kernel features reside. It itself is not a git repository but rather hosts multiple kernel features which are
* **kernel/runtime** is optionally used during test runtime to mimic virtual storage. For example, if your code is meant to generate a sub-tree in procfs or sysfs - you can re-create it in **kernel/runtime**. This is nice for debugging.<br>
  **NOTE**: Sub-tree verification does not require physically creating it in
  **kernel/runtime** but it's convenient for debugging so doing so is an optional addition to such tests. Whether sub-trees are to be re-created in **kernel/runtime** or not is decided by CONFIG_KUT_FS provided to kunit at compile time

### Test Flow
Following is how kunit conducts its tests.<br>
Each tested kernel feature (git repository under **ut**) includes a *test package* of numerous *tests* to be run by kunit:

<p align="left">
  <img src="https://raw.githubusercontent.com/k-unit/core/master/images/kunit_test_flow.png" width="500"/>
</p>

## Setup and Build
As mentioned above, this is not a stand alone project and as such it is not intended to be cloned directly but rather as part of a larger multi-git project. Please use the [setup](https://github.com/k-unit/setup.git "k-unit Multi-Git Setup Repository") repository to set kunit up!<br>
<br>
To setup the project we use the [Android *repo* tool](https://storage.googleapis.com/git-repo-downloads/repo "Android repo Tool"). See the [setup](https://github.com/k-unit/setup.git "k-unit Multi-Git Setup Repository") repository for information on getting *repo*. 

### Setup Stages:
 1. Create your *kunit project root directory* and from within it issue:<br>
    `repo init -u https://github.com/k-unit/setup`<br>
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
The following options are supported by *kunit*:

* **-l, --list [name1 name2 ...]**<br>
  <br>
  List available unit tests<br>
  <br>
  If no test names are specified then a summary of all available tests is listed.<br>
  Example:<br>
  
  `./kunit --list`
  
  | Num  | description            | name        |
  |:----:|:---------------------- |:----------- |
  |  1.  | Kernel Emulation       | kernel      |
  |  2.  | Sliding Window         | slw         |
  |  3.  | Device Tree Hierarchy  | dev_tree    |
  |  4.  | Various debug FS files | mmc_debugfs |
  |  5.  | Field Firmware Upgrade | ffu         |
  
  Specific test names may be used in which case they will be listed in more detail. The available test names are those listed when using --list without arguments.<br>
  Example:<br>
  
  `./kunit --list slw ffu`
  
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
  <tr><td>./kunit</td> <td>all tests will run</td></tr>
  <tr><td>./kunit dev_tree</td> <td>all dev_tree unit tests will run</td></tr>
  <tr><td>./kunit ffu 2 4</td> <td>ffu tests #2 and #4 will run</td></tr>
  </table>

## Adding New Tests
Adding new tests to an existing *test package* is designed to be simple and straight forward.<br>

The simplicity of adding new tests depends on there being sufficient mock and utility function support within kunit. If there is lacking mock or utility support - it will need to be added (and tested) before you can use it in tests. This can vary from being quite trivial to fairly complex.

Adding a new *test package* to kunit requires:
* Importing kernel feature and placing it in **ut**
* Implementing a set of tests for the kunit framework<br>
  The feature's test object-file must be added to the *obj-ut* target in the feature's *Kbuild* file
* In the [env](https://github.com/k-unit/env.git "Environment setup and build") repository
  * List the feature patches which are intended to be applied in the kernel/kunit to allow interfacing with it in *patches*. All patches listed in *patches* will be applied at **kernel/patches** during the project setup
  * Add any required `CONFIG=` entries to *build/kunit_defconfig*. This file
    gets copied to kunit root directory and renamed to *.config* at during setup
  * Enable/Disable the building in of the feature in *build/Kbuild*. This file
    gets copied to *ut/Kbuild* during setup
* In the [core](https://github.com/k-unit/core "A framework for unit-testing kernel in user space") repository (this one)
  * Link each feature to the core for testing in *tests.h*

### kunit Inter-component Interactions
*kunit* consists of five main code components:
  1. **kernel/emu/lnx** - kernel API mock functions, constants, macros, etc...
  2. **kernel/emu/kut** - framework utility functions
  3. **ut/feature** kernel code - feature implementation for the kernel, this is
     the code we're out to test
  4. **ut/feature** test code - unit tests for the feature
  5. **kernel/patches** - feature's API (\*.h files) for the kernel patched in from **ut/feature**

These components have a *can use* relation between them.<br>
The following table states for each code component in the left-most column,
whether it can or can't call directly to code in each of the other components represented in the remaining columns:

<table>
  <tr align="center">
    <th></th>
    <th>kernel/emu/lnx</th>
    <th>kernel/emu/kut</th>
    <th>ut/feature - kernel</th>
    <th>ut/feature - tests</th>
    <th>kernel/patches</th>
  </tr>
  <tr align="center">
    <td align="left"><b>kernel/emu/lnx</b></td>
    <td>yes</td>
    <td>yes</td>
    <td>no</td>
    <td>no</td>
    <td>no</td>
  </tr>
  <tr align="center">
    <td align="left"><b>kernel/emu/kut</b></td>
    <td>yes</td>
    <td>yes</td>
    <td>no</td>
    <td>no</td>
    <td>no</td>
  </tr>
  <tr align="center">
    <td align="left"><b>ut/feature - kernel</b></td>
    <td>yes</td>
    <td>no</td>
    <td>yes</td>
    <td>no</td>
    <td>yes</td>
  </tr>
  <tr align="center">
    <td align="left"><b>ut/feature - tests</b></td>
    <td>yes</td>
    <td>yes</td>
    <td>yes</td>
    <td>yes</td>
    <td>yes</td>
  </tr>
  <tr align="center">
    <td align="left"><b>kernel/patches</b></td>
    <td colspan="5">N/A</td>
  </tr>
</table>

