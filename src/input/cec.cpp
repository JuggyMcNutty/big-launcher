#include "cec.hpp"
#include "../render/logger.hpp"

#ifdef FOYER_CEC
#include <libcec/cec.h>

static void cec_client_cb(void *userdata, cec_client_info_t *info)
{
    (void) info;
    // Map CEC messages to actions here (Phase 3).
    (void) userdata;
}

Foyer::Cec::Cec() = default;

Foyer::Cec::~Cec()
{
    if (cec_)
        cec_client_destroy((cec_client_t*) cec_);
}

bool Foyer::Cec::init(const char *adapter)
{
    cec_client_options_t options = cec_client_default_options();
    options.loglevel = CEC_LOG_LEVEL_ERROR;
    options.client_callback = cec_client_cb;
    options.userdata = this;
    cec_ = cec_client_create(adapter ? adapter : "/dev/cec0");
    if (!cec_) {
        Foyer::logger::warn("CEC: no adapter found");
        return false;
    }
    cec_client_start((cec_client_t*) cec_);
    Foyer::logger::info("CEC: initialized");
    return true;
}

void Foyer::Cec::standby()
{
    if (cec_)
        cec_client_send_power_off((cec_client_t*) cec_);
}

void Foyer::Cec::active()
{
    if (cec_)
        cec_client_send_user_control_key((cec_client_t*) cec_, CEC_USER_CONTROL_KEY_POWER);
}
#else
Foyer::Cec::Cec() = default;
Foyer::Cec::~Cec() = default;
bool Foyer::Cec::init(const char *) { return false; }
void Foyer::Cec::standby() {}
void Foyer::Cec::active() {}
#endif
