#ifndef MCD_UTILS_H
#define MCD_UTILS_H

#include "mcd_api.h"
#include "mcd_tools.h"
#include "mcd_loader_class.h"


//-------------------------------------------------------------------------------------------------
// Global MCD API loader class pointer
extern McdLoaderClass* mcd;

//-------------------------------------------------------------------------------------------------
void check_core_status(mcd_core_st *core, mcd_core_con_info_st *core_con_info_core, mcd_core_state_st *state, 
	mcd_register_info_st *core_ip_reg, uint32_t *core_ip);

void mcdd_get_core_ip_addr(mcd_core_st *core, mcd_register_info_st *core_ip_reg);

void mcdd_handle_err(FILE *lf, mcd_core_st **core, mcd_return_et ret);

mcd_return_et mcdd_read_core_ip(mcd_core_st *core, const mcd_register_info_st *core_ip_reg, 
                                uint32_t *core_ip);

//-------------------------------------------------------------------------------------------------
// Server related functions
void mcdd_open_servers(const char *system_key, const char *config_string,
                       uint32_t *num_servers, mcd_server_st *server);

void mcdd_select_running_server(const char *host, const char *system_key, uint32_t *num_servers, 
                                mcd_server_st **server);

void mcdd_start_servers(const char *host, const char *system_key, uint32_t *num_servers, 
                        mcd_server_st **server);

mcd_return_et mcdd_set_acc_hw_frequency(mcd_server_st *server, uint32_t frequ);

#endif /* MCD_UTILS_H */