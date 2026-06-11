#ifndef SPARTA_ISTHMUS_ABLATION_BRIDGE_H
#define SPARTA_ISTHMUS_ABLATION_BRIDGE_H

#include "isthmus_ablation/model.hpp"

#include <string>
#include <vector>

namespace SPARTA_NS {

class SPARTA;

namespace IACBridge {

iac::Config &config();
bool owns_model(SPARTA *sparta);
iac::Model &model(SPARTA *sparta);
void reset_model();
void set_coupling_interval_from_dsmc(SPARTA *sparta);
void set_last_coupling_step(SPARTA *sparta);
void generate_surface(SPARTA *sparta, const iac::IsthmusSurfaceCommand &surface);
const std::vector<iac::Model::PublicSurfaceTriangle> &
surface_triangles(SPARTA *sparta, const std::string &surface_id);
void install_surface(SPARTA *sparta, const char *surface_id, int partflag, int type);
void reset_stats_output();
void print_stats_after_step(SPARTA *sparta);

} // namespace IACBridge

} // namespace SPARTA_NS

#endif
