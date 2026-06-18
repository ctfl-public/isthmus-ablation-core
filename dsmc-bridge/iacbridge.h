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
bool has_model();
iac::Model &model(SPARTA *sparta);
void reset_model();
void set_coupling_interval_from_dsmc(SPARTA *sparta);
void set_last_coupling_step(SPARTA *sparta);
void generate_surface(SPARTA *sparta, const iac::IsthmusSurfaceCommand &surface);
const std::vector<iac::Model::PublicSurfaceTriangle> &
surface_triangles(SPARTA *sparta, const std::string &surface_id);
void install_surface(SPARTA *sparta, const char *surface_id, int partflag, int type);
void reset_stats_output();
void record_grid_vtu_dump(const std::string &fix_id, const std::string &path,
                          const std::string &index_mode,
                          const std::vector<std::string> &fields, int every = 0);
void write_scheduled_grid_vtu_dumps(SPARTA *sparta);
void print_compact_text(SPARTA *sparta, const std::string &text);
void print_coupled_summary(SPARTA *sparta);
void print_stats_after_step(SPARTA *sparta);
void print_sparta_stats(SPARTA *sparta);
void error_if_root_failed(SPARTA *sparta, const std::string &message);

} // namespace IACBridge

} // namespace SPARTA_NS

#endif
