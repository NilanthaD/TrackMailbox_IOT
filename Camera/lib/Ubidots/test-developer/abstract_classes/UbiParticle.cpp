#include "Particle.h"
#include "UbiParticle.h"
#include "UbiConstants.h"

UbiParticle::UbiParticle(const char* host, const int port, const char* user_agent, const char* token){
  _host = host;
  _user_agent = user_agent;
  _token = token;
}

bool UbiParticle::sendData(const char* device_label, const char* device_name, char* payload, Ubi_flags* flags){
  return Particle.publish(device_label, payload, flags->particle_flag);
}

float UbiParticle::get(const char* device_label, const char* variable_label) {
  return ERROR_VALUE;
}

void UbiParticle::setDebug(bool debug){
  _debug = debug;
}