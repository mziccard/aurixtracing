/*
 * Copyright (c) 2015 Marco Ziccardi
 * Licensed under the MIT license.
 */

#include <stdio.h>
#include <string>
#include <sstream>
#include <assert.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>
#include <vector>

namespace po = boost::program_options;

#include "mcd_utils.h"

#ifdef _WIN32
    #include <windows.h>

    void sleep(unsigned milliseconds)
    {
        Sleep(milliseconds);
    }
#else
    #include <unistd.h>

    void sleep(unsigned milliseconds)
    {
        usleep(milliseconds * 1000); // takes microseconds
    }
#endif

unsigned int TRACE_BUFFER_SIZE = 1024;
unsigned int TRACE_BUFFER_ADDRESS = 0xD0000000;
unsigned int BUFFER_READ_INDEX_OFFSET = 0;
unsigned int BUFFER_WRITE_INDEX_OFFSET = 2;
unsigned int TRACE_BUFFER_OFFSET = 4;
unsigned int TRACE_BUFFER_ENTRY_SIZE = 12;
unsigned int TRACE_BUFFER_TIMESTAMP_OFFSET = 4;


McdLoaderClass* mcd;

//-------------------------------------------------------------------------------------------------
int main(int argc, char** argv) 
{

  // We assume server host is running on localhost
  char  host[64];
  std::string output = "trace_";
  strcpy(host, "localhost");
  bool single_core_trace = false;
  unsigned int polling_period = 1000;

  try {
    po::options_description desc("Allowed options");
    desc.add_options()
      ("help,h", "produce help message")
      ("buffer-address,a", po::value<unsigned int>(), "Address of print buffer")
      ("polling-period,p", po::value<unsigned int>(), "Polling period in microseconds")
      ("server-address,s", po::value<std::string>(), "IP of the MCD server")
      ("output,o", po::value<std::string>(), "Trace file suffix")
      ("singlecore,c", po::value<bool>()->implicit_value(true), "Trace only master core (0)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    if (vm.count("buffer-address")) {
        TRACE_BUFFER_ADDRESS = vm["buffer-address"].as<unsigned int>();
    }

    if (vm.count("server-address")) {
      strcpy(host, vm["server-address"].as<std::string>().c_str());
    }

    if (vm.count("output")) {
      output = vm["output"].as<std::string>().c_str();
    }

    if (vm.count("singlecore")) {
      single_core_trace = vm["singlecore"].as<bool>();
    }

    if (vm.count("polling-period")) {
      polling_period = vm["singlecore"].as<unsigned int>();
    }

  } catch(po::error& e) {
    std::cout << e.what() << "\n" << "Use --help/-h for help."<< "\n";
    return 2;
  }

  mcd_return_et         ret;
  mcd_core_con_info_st  coreConInfo;
  uint32_t              i, sv, tmp, numOpenServers, numSystems;

  mcd_api_version_st versionReq;
  versionReq.v_api_major  = MCD_API_VER_MAJOR;
  versionReq.v_api_minor  = MCD_API_VER_MINOR;
  strcpy(versionReq.author, MCD_API_VER_AUTHOR);

  // We use mcdxdas.dll as MCD API library
  char  lib_name[64];
  strcpy(lib_name, "mcdxdas.dll");
  
  // Load and initialize MCD API 
  mcd_impl_version_info_st mcd_impl_info;
  assert (mcd == 0);
  mcd = new McdLoaderClass(lib_name);
  assert(mcd->lib_loaded());
  ret = mcd->mcd_initialize_f(&versionReq, &mcd_impl_info);
  assert(ret == MCD_RET_ACT_NONE);
  mcdt_print_mcd_impl_info(stdout, &mcd_impl_info);

  // System key
  char system_key[MCD_KEY_LEN];
  system_key[0] = 0;

  // Query number of running servers and start server if none is running
  numOpenServers = 0;
  ret = mcd->mcd_qry_servers_f(host, TRUE, 0, &numOpenServers, 0);
  assert(ret == MCD_RET_ACT_NONE);

  const uint32_t maxNumServers = 16;
  mcd_server_st *openServers[maxNumServers];
  if (numOpenServers == 0) {
    numOpenServers = maxNumServers;
    mcdd_start_servers(host, system_key, &numOpenServers, openServers);
  }
  else {
    numOpenServers = maxNumServers;
    mcdd_select_running_server(host, system_key, &numOpenServers, openServers);
  }

  // Select system
  uint32_t  i_system = 0;
  mcd_core_con_info_st core_con_info_system_common;
  tmp = 1;
  ret = mcd->mcd_qry_systems_f(i_system, &tmp, &core_con_info_system_common);
  assert(ret == MCD_RET_ACT_NONE);

  // Number of devices
  uint32_t num_devices = 0;
  ret = mcd->mcd_qry_devices_f(&core_con_info_system_common, 0, &num_devices, 0);
  assert(ret == MCD_RET_ACT_NONE);
  assert(num_devices > 0);


  printf("Found %d devices within system %s\n\n", num_devices, core_con_info_system_common.system);

  for (i = 0; i < num_devices; i++) {
    tmp = 1;
    ret = mcd->mcd_qry_devices_f(&core_con_info_system_common, i, &tmp, &coreConInfo);
    assert(ret == MCD_RET_ACT_NONE);
    mcdt_print_core_con_info(stdout, &coreConInfo);
    printf("\n");
  }

  // Select device
  uint32_t  i_device = 0;
  if (num_devices > 1) {
    printf("\nEnter device index (0...%d):\n", num_devices - 1);
    scanf("%i", &i_device);
  }

  mcd_core_con_info_st core_con_info_device_common;
  tmp = 1;
  ret = mcd->mcd_qry_devices_f(&core_con_info_system_common, i_device, &tmp, 
                          &core_con_info_device_common);
  assert(ret == MCD_RET_ACT_NONE);

  // Number of cores
  uint32_t num_cores = 0;  // Just to get the number of cores
  ret = mcd->mcd_qry_cores_f(&core_con_info_device_common, 0, &num_cores, 0);
  assert(ret == MCD_RET_ACT_NONE);
  assert(num_cores >= 1);

  printf("Found %d cores within device %s\n\n", num_cores, core_con_info_device_common.device);

  for (i = 0; i < num_cores; i++) {
    tmp = 1;
    ret = mcd->mcd_qry_cores_f(&core_con_info_device_common, i, &tmp, &coreConInfo);
    assert(ret == MCD_RET_ACT_NONE);
    assert(strcmp(coreConInfo.host, core_con_info_device_common.host) == 0);
    assert(strcmp(coreConInfo.system_key, core_con_info_device_common.system_key) == 0);
    assert(coreConInfo.device_key[0] == 0);  // Safe assumption for models
    assert(strcmp(coreConInfo.system, core_con_info_device_common.system) == 0);
    mcdt_print_core_con_info(stdout, &coreConInfo);
    printf("\n");
  }

  // We select all cores, traces are stored in local memory

  // Cores ids
  // No need to select core for SimIO
  //if (num_cores > 1) {
  //  printf("\nEnter core index (0...%d):\n", num_cores - 1);
  //  scanf("%i", &i_core);
  //}

  if (single_core_trace) 
    num_cores = 1;

  // We load all core infos
  std::vector<mcd_core_con_info_st> core_con_infos(num_cores);
  for (unsigned int core_id = 0; core_id < num_cores; core_id++) {
    tmp = 1;
    ret = mcd->mcd_qry_cores_f(&core_con_info_device_common, core_id, &tmp, &core_con_infos[core_id]);
    assert(ret == MCD_RET_ACT_NONE);
  }

  // We open all cores
  std::vector<mcd_core_st *> cores(num_cores);
  std::vector<mcd_register_info_st> core_ip_regs(num_cores);
  for (unsigned int core_id = 0; core_id < num_cores; core_id++) {
    ret = mcd->mcd_open_core_f(&core_con_infos[core_id], &cores[core_id]);
    mcdd_handle_err(stdout, 0, ret);
    assert(cores[core_id] != NULL);

    mcdd_get_core_ip_addr(cores[core_id], &core_ip_regs[core_id]);

    // Use strongest reset
    uint32_t rstClassVectorAvail, rstClassVector = 1; 
    ret = mcd->mcd_qry_rst_classes_f(cores[core_id], &rstClassVectorAvail);
    assert(ret == MCD_RET_ACT_NONE);
    assert(rstClassVectorAvail & rstClassVector);
  }

  uint32_t  value, core_ip;

  // tx and txlist setup
  mcd_tx_st txDemo;
  memset(&txDemo, 0, sizeof(txDemo));  // Set all to default values
  mcd_txlist_st  txlistDemo;
  txlistDemo.tx     = &txDemo;
  txlistDemo.num_tx = 1;
  txDemo.num_bytes = sizeof(value);
  txDemo.data      = (uint8_t*) &value;

  // trig setup
  mcd_trig_simple_core_st trigDemo;
  memset(&trigDemo, 0, sizeof(mcd_trig_simple_core_st));  // Set all to default values
  trigDemo.struct_size = sizeof(mcd_trig_simple_core_st);
  trigDemo.type        = MCD_TRIG_TYPE_IP;
  trigDemo.action      = MCD_TRIG_ACTION_DBG_DEBUG;
  trigDemo.option      = MCD_TRIG_OPT_DEFAULT;
  trigDemo.addr_range  = 0;  // Single IP trigger

  mcd_core_state_st  state;
  core_ip = 0xEEEEEEEE;
  state.state = MCD_CORE_STATE_UNKNOWN;

  std::vector<std::ofstream> trace_streams(num_cores);
  for (unsigned int core_id = 0; core_id < num_cores; core_id++) {
    trace_streams[core_id].open(output+"core"+std::to_string(static_cast<long long>(core_id))+".txt");
  }

  while(true) {

    for (unsigned int core_id = 0; core_id < num_cores; core_id++) {
      // Check if the device is still connected are still connected
      check_core_status(cores[core_id], &core_con_infos[core_id], &state, &core_ip_regs[core_id], &core_ip);

      // Read and write indexes in the Target to Host buffer
      uint16_t read_index = 0;
      uint16_t write_index = 0;

      // Read the read index
      txDemo.addr.address = TRACE_BUFFER_ADDRESS + BUFFER_READ_INDEX_OFFSET; 
      ret = mcd->read16(cores[core_id], &txDemo.addr, &read_index);
      mcdd_handle_err(stdout, &cores[core_id], ret);

      // Read the write index
      txDemo.addr.address = TRACE_BUFFER_ADDRESS + BUFFER_WRITE_INDEX_OFFSET; 
      ret = mcd->read16(cores[core_id], &txDemo.addr, &write_index);
      mcdd_handle_err(stdout, &cores[core_id], ret);

      if (read_index != write_index) {

        if (read_index > write_index) {
          // Print each character not yet readn in the buffer
          while(read_index < TRACE_BUFFER_SIZE) {
	          
            uint32_t id = 0;
            uint64_t timestamp = 0;

            txDemo.addr.address = TRACE_BUFFER_ADDRESS + TRACE_BUFFER_OFFSET + read_index*TRACE_BUFFER_ENTRY_SIZE;
	          ret = mcd->read32(cores[core_id], &txDemo.addr, &id);
            mcdd_handle_err(stdout, &cores[core_id], ret);

            txDemo.addr.address = TRACE_BUFFER_ADDRESS + TRACE_BUFFER_OFFSET + read_index*TRACE_BUFFER_ENTRY_SIZE 
              + TRACE_BUFFER_TIMESTAMP_OFFSET;
	          ret = mcd->read64(cores[core_id], &txDemo.addr, &timestamp);
            mcdd_handle_err(stdout, &cores[core_id], ret);

            trace_streams[core_id] << id << " " << timestamp << std::endl;
            trace_streams[core_id].flush();
	          read_index++;
          }
          read_index = 0;
        }

        // Print each character not yet readn in the buffer
        while(read_index < write_index) {

            uint32_t id = 0;
            uint64_t timestamp = 0;

            txDemo.addr.address = TRACE_BUFFER_ADDRESS + TRACE_BUFFER_OFFSET + read_index*TRACE_BUFFER_ENTRY_SIZE;
	          ret = mcd->read32(cores[core_id], &txDemo.addr, &id);
            mcdd_handle_err(stdout, &cores[core_id], ret);

            txDemo.addr.address = TRACE_BUFFER_ADDRESS + TRACE_BUFFER_OFFSET + read_index*TRACE_BUFFER_ENTRY_SIZE 
              + TRACE_BUFFER_TIMESTAMP_OFFSET;
	          ret = mcd->read64(cores[core_id], &txDemo.addr, &timestamp);
            mcdd_handle_err(stdout, &cores[core_id], ret);

            trace_streams[core_id] << id << " " << timestamp << std::endl;
            trace_streams[core_id].flush();
	          read_index++;
        }

        // Update read index
        txDemo.addr.address = TRACE_BUFFER_ADDRESS + BUFFER_READ_INDEX_OFFSET; 
        ret = mcd->write16(cores[core_id], &txDemo.addr, write_index);
        mcdd_handle_err(stdout, &cores[core_id], ret);
      }
    }

    sleep(polling_period/1000);
  }

  // Wait for user input before closing
  scanf("%i", &ret);

  // Close cores and output files
  for (unsigned int core_id = 0; core_id < num_cores; core_id++) {
    ret = mcd->mcd_close_core_f(cores[core_id]);
    assert(ret == MCD_RET_ACT_NONE);
    // Close output file
    trace_streams[core_id].close();
  }



  // Cleanup
  mcd->mcd_exit_f(); // Enforce cleanup of all core and server connections 
  delete mcd;        // Unloads lib (destructor of McdLoaderClass)

  return 0;
}