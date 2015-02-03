#include "mcd_utils.h"
#include <assert.h>

/**
 * Check whether the device is still connected and is in a correct status
 **/
void check_core_status(mcd_core_st *core, mcd_core_con_info_st *core_con_info_core, mcd_core_state_st *state, 
	mcd_register_info_st *core_ip_reg, uint32_t *core_ip) {
  mcd_return_et         ret, ret1;
  if (core == NULL) {  // E.g. miniWiggler was unplugged
	// Trying to reconnect core
    ret = mcd->mcd_open_core_f(core_con_info_core, &core);
    if (ret == MCD_RET_ACT_NONE)
	  printf("Core successfully reconnected\n");
    else
	  mcdd_handle_err(stdout, &core, ret);
  }

  if (core != NULL) {
	ret1 = mcd->mcd_qry_state_f(core, state);
	if (ret1 != MCD_RET_ACT_HANDLE_ERROR)  // Avoid double notification
	  ret1 = mcdd_read_core_ip(core, core_ip_reg, core_ip);
	mcdd_handle_err(stdout, &core, ret1);
  }  
}


//-------------------------------------------------------------------------------------------------
void mcdd_get_core_ip_addr(mcd_core_st *core, mcd_register_info_st *core_ip_reg)
{
  mcd_return_et ret;
  uint32_t reg_group_id, i, num_regs, num_regs_tmp;

  reg_group_id = 0; // Default (at least for TriCore, XMC4000, XC2000, XE166 and XC800)

  num_regs = 0; // Just query number
  ret = mcd->mcd_qry_reg_map_f(core, reg_group_id, 0, &num_regs, core_ip_reg);
  assert(ret == MCD_RET_ACT_NONE);

  for (i = 0; i < num_regs; i++) {
    num_regs_tmp = 1;
    ret = mcd->mcd_qry_reg_map_f(core, reg_group_id, i, &num_regs_tmp, core_ip_reg);
    assert(ret == MCD_RET_ACT_NONE);
    if (   (strcmp(core_ip_reg->regname, "PC") == 0) 
        || (strcmp(core_ip_reg->regname, "IP") == 0) ) {
      break;
    }
  }
  assert(i < num_regs);
}


//-------------------------------------------------------------------------------------------------
void mcdd_handle_err(FILE *lf, mcd_core_st **core, mcd_return_et ret)
{
  if (ret == MCD_RET_ACT_NONE)
    return;

  mcd_error_info_st errInfo;

  if (core == NULL)
    mcd->mcd_qry_error_info_f(NULL, &errInfo);
  else
    mcd->mcd_qry_error_info_f(*core, &errInfo);

  // Handle events
  if (errInfo.error_events & MCD_ERR_EVT_RESET)
    fprintf(lf, "EVENT: Target has been reset\n");
  if (errInfo.error_events & MCD_ERR_EVT_PWRDN)
    fprintf(lf, "EVENT: Target has been powered down\n");
  if (errInfo.error_events & MCD_ERR_EVT_HWFAILURE)
    fprintf(lf, "EVENT: There has been a target hardware failure\n");
  if (errInfo.error_events & ~7) { // Not MCD_ERR_EVT_RESET, _PWRDN, _HWFAILURE
    assert(false);
    fprintf(lf, "EVENT: There has been an unknown event\n");
  }

  if (ret == MCD_RET_ACT_HANDLE_EVENT) {
    return;  // Nothing to do
  }
  
  assert(ret == MCD_RET_ACT_HANDLE_ERROR);
  assert(errInfo.error_str[0] != 0);
  fprintf(lf, "ERROR: %s\n", errInfo.error_str);

  if (errInfo.error_code == MCD_ERR_CONNECTION) {  // E.g. miniWiggler was unplugged
    if ((core != NULL) && (*core != NULL)) {
      mcd->mcd_close_core_f(*core);
      *core = NULL;  // Will try to reconnect in main loop
    }
  }
}


//-------------------------------------------------------------------------------------------------
// The number of opened servers depends on how specific the config string is.
// E.g. in case of Real HW, whether it contains the name of the tool Access HW.
void mcdd_open_servers(const char *system_key, const char *config_string,
                           uint32_t *num_servers, mcd_server_st **server)
{
  mcd_return_et ret;
  uint32_t sv;

  printf("\nOpen Servers\n\n");

  for (sv = 0; sv < *num_servers; sv++) { 

    ret = mcd->mcd_open_server_f(system_key, config_string, &server[sv]);

    if (ret != MCD_RET_ACT_NONE) {
      assert(ret == MCD_RET_ACT_HANDLE_ERROR);
      mcd_error_info_st errInfo;
      mcd->mcd_qry_error_info_f(0, &errInfo);
      break;  // while
    }
    printf("%s\n", server[sv]->config_string);
  }

  *num_servers = sv;
}

//-------------------------------------------------------------------------------------------------
mcd_return_et mcdd_read_core_ip(mcd_core_st *core, const mcd_register_info_st *core_ip_reg, 
                                uint32_t *core_ip)
{
  mcd_return_et ret = MCD_RET_ACT_HANDLE_ERROR;

  switch (core_ip_reg->regsize) {
    case 32:
      ret = mcd->read32(core, &core_ip_reg->addr, core_ip);
      break;
    case 16:
      uint16_t core_ip16;
      ret = mcd->read16(core, &core_ip_reg->addr, &core_ip16);
      *core_ip = core_ip16;
    break;
    default:
      assert(false);
  }
  return ret;
}


//-------------------------------------------------------------------------------------------------
mcd_return_et mcdd_set_acc_hw_frequency(mcd_server_st *server, uint32_t frequ)
{
  mcd_return_et ret;

  uint32_t frequNew = frequ;
  ret = mcd->set_acc_hw_frequency(server, &frequNew);

  if (ret == MCD_RET_ACT_NONE)
    printf("Frequency set to %d kHz\n", frequNew/1000);
  else
    printf("Could not set frequency\n");
 
  return ret;
}


//-------------------------------------------------------------------------------------------------
// In mcdxdas.dll V1.4.0 the standard approach doesn't work due to an implementation bug
void mcdd_set_acc_hw_frequency_mcdxdas_v140_workaround(mcd_core_st *core, uint32_t frequ)
{
  // Using some magic...
  mcd_addr_st addr;
  memset(&addr, 0, sizeof(mcd_addr_st));
  addr.address      = 0x000C0100;
  addr.addr_space_id = 0xDADADA84;

  mcd_return_et ret;
  ret = mcd->write32(core, &addr, frequ);
  assert(ret == MCD_RET_ACT_NONE);

  uint32_t frequNew;
  ret = mcd->read32(core, &addr, &frequNew);
  assert(ret == MCD_RET_ACT_NONE);

  printf("Frequency set to %d kHz\n", frequNew/1000);
}


//-------------------------------------------------------------------------------------------------
void mcdd_select_running_server(const char *host, const char *system_key, uint32_t *num_servers, 
                                mcd_server_st **server)
{ 
  mcd_return_et ret;

  uint32_t i, sv, numRunningServers, notOnlyTheSelectedAccHw;
  const uint32_t maxNumServers = 16;
  mcd_server_info_st serverInfo[maxNumServers];

  numRunningServers = maxNumServers;
  ret = mcd->mcd_qry_servers_f(host, TRUE, 0, &numRunningServers, serverInfo);
  assert(ret == MCD_RET_ACT_NONE);

  printf("\nRunning Servers\n");
  mcdt_print_server_info(stdout, numRunningServers, serverInfo);
  printf("\n");

  if (numRunningServers > 1) {
    printf("\nEnter server index (0...%d):\n", numRunningServers - 1);
    scanf("%i", &sv);
    assert(sv < numRunningServers);

    // Check if there are different Access HWs for the same kind of server
    for (i = 0; i < numRunningServers; i++) {
      if (i == sv)
        continue;
      if (serverInfo[i].acc_hw[0] == 0)
        continue;
      if (strncmp(serverInfo[i].server, serverInfo[sv].server, MCD_UNIQUE_NAME_LEN) == 0)
        break;
    }
    if (i < numRunningServers) {
      printf("\nEnter 0 to open only the selected Access HW server:\n");
      scanf("%i", &notOnlyTheSelectedAccHw);
    }
  }
  else {
    assert(numRunningServers == 1);
    sv = 0;
  }

  char  configString[256];
  if (serverInfo[sv].acc_hw[0] != 0) {  // Real HW
    assert(serverInfo[sv].system_instance[0] == 0); 
    if (notOnlyTheSelectedAccHw)
      sprintf(configString, "McdHostName=\"%s\"\nMcdServerName=\"%s\" ", 
              host, serverInfo[sv].server);
    else
      sprintf(configString, "McdHostName=\"%s\"\nMcdServerName=\"%s\"\nMcdAccHw=\"%s\" ", 
              host, serverInfo[sv].server, serverInfo[sv].acc_hw);
  }
  else if (serverInfo[sv].system_instance[0] != 0) {  // Simulation model
    assert(serverInfo[sv].acc_hw[0] == 0); 
    sprintf(configString, "McdHostName=\"%s\"\nMcdServerName=\"%s\"\nMcdSystemInstance=\"%s\" ", 
            host, serverInfo[sv].server, serverInfo[sv].system_instance);
  } else {  // Not a good MCD API implementation
    assert(false);
    sprintf(configString, "McdHostName=\"%s\"\nMcdServerName=\"%s\" ", 
            host, serverInfo[sv].server);
  }

  mcdd_open_servers(system_key, configString, num_servers, server);
}


//-------------------------------------------------------------------------------------------------
void mcdd_start_servers(const char *host, const char *system_key, uint32_t *num_servers, 
                        mcd_server_st **server)
{
  mcd_return_et ret;
  uint32_t      sv, numInstalledServers;

  const uint32_t maxNumInstalledServers = 16;
  mcd_server_info_st serverInfo[maxNumInstalledServers];

  // Query installed servers
  numInstalledServers = maxNumInstalledServers;
  ret = mcd->mcd_qry_servers_f(host, FALSE, 0, &numInstalledServers, serverInfo);
  assert(ret == MCD_RET_ACT_NONE);

  printf("\nInstalled Servers:\n");
  mcdt_print_server_info(stdout, numInstalledServers, serverInfo);

  printf("\n\nEnter server index (0...%d) to start server\n", num_servers - 1);

  scanf("%i", &sv);

  assert(sv < numInstalledServers);

  char configString[128];
  sprintf(configString, "McdHostName=\"%s\"\nMcdServerName=\"%s\" ", host, serverInfo[sv].server);
  
  // In case of Real HW, servers for all different Access HWs will be openend
  // If several boards are connected, all devices will be available for the selection process
  mcdd_open_servers(system_key, configString, num_servers, server);
}