#include "gtest/gtest.h"

#include "nigiri/loader/gtfs/load_timetable.h"
#include "nigiri/loader/init_finish.h"
#include "nigiri/rt/create_rt_timetable.h"
#include "nigiri/rt/rt_timetable.h"
#include "nigiri/timetable.h"

#include "osr/extract/extract.h"
#include "osr/lookup.h"
#include "osr/platforms.h"
#include "osr/routing/route.h"
#include "osr/ways.h"

#include "icc/compute_footpaths.h"
#include "icc/elevators/elevators.h"
#include "icc/elevators/match_elevator.h"
#include "icc/endpoints/routing.h"
#include "icc/get_loc.h"
#include "icc/match_platforms.h"
#include "icc/tt_location_rtree.h"

/*
#include "fmt/core.h"

#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/program_options.hpp"

#include "net/web_server/query_router.h"
#include "net/web_server/web_server.h"

#include "utl/read_file.h"

#include "net/run.h"

#include "osr/lookup.h"

#include "nigiri/rt/create_rt_timetable.h"
#include "nigiri/rt/rt_timetable.h"

#include "icc/elevators/elevators.h"
#include "icc/endpoints/elevators.h"
#include "icc/endpoints/footpaths.h"
#include "icc/endpoints/graph.h"
#include "icc/endpoints/levels.h"
#include "icc/endpoints/matches.h"
#include "icc/endpoints/osr_routing.h"
#include "icc/endpoints/platforms.h"
#include "icc/endpoints/routing.h"
#include "icc/endpoints/update_elevator.h"
#include "icc/match_platforms.h"
#include "icc/point_rtree.h"
#include "icc/tt_location_rtree.h"

namespace asio = boost::asio;
namespace n = nigiri;
namespace fs = std::filesystem;
namespace bpo = boost::program_options;
namespace json = boost::json;
*/

namespace n = nigiri;
namespace nl = nigiri::loader;
namespace json = boost::json;
using namespace std::string_view_literals;
using namespace icc;
using namespace date;

constexpr auto const kFastaJson = R"__(
[
  {
    "description": "FFM HBF zu Gleis 101/102 (S-Bahn)",
    "equipmentnumber" : 10561326, "geocoordX" : 8.6628995,
    "geocoordY" : 50.1072933, "operatorname" : "DB InfraGO",
    "state" : "ACTIVE",
    "stateExplanation" : "available",
    "stationnumber" : 1866,
    "type" : "ELEVATOR"
  },
  {
    "description": "FFM HBF zu Gleis 103/104 (S-Bahn)",
    "equipmentnumber": 10561327,
    "geocoordX": 8.6627516,
    "geocoordY": 50.1074549,
    "operatorname": "DB InfraGO",
    "state": "ACTIVE",
    "stateExplanation": "available",
    "stationnumber": 1866,
    "type": "ELEVATOR"
  },
  {
    "description": "HAUPTWACHE zu Gleis 2/3 (S-Bahn)",
    "equipmentnumber": 10351032,
    "geocoordX": 8.67818,
    "geocoordY": 50.114046,
    "operatorname": "DB InfraGO",
    "state": "ACTIVE",
    "stateExplanation": "available",
    "stationnumber": 1864,
    "type": "ELEVATOR"
  },
  {
    "description": "DA HBF zu Gleis 1",
    "equipmentnumber": 10543458,
    "geocoordX": 8.6303864,
    "geocoordY": 49.8725612,
    "state": "ACTIVE",
    "type": "ELEVATOR"
  },
  {
    "description": "DA HBF zu Gleis 3/4",
    "equipmentnumber": 10543453,
    "geocoordX": 8.6300911,
    "geocoordY": 49.8725678,
    "operatorname": "DB InfraGO",
    "state": "ACTIVE",
    "stateExplanation": "available",
    "stationnumber": 1126,
    "type": "ELEVATOR",
    "outOfService": [["2024-07-18T12:00:00Z", "2024-07-19T12:00:00Z"]]
  },
  {
    "description": "zu Gleis 5/6",
    "equipmentnumber": 10543454,
    "geocoordX": 8.6298163,
    "geocoordY": 49.8725555,
    "operatorname": "DB InfraGO",
    "state": "ACTIVE",
    "stateExplanation": "available",
    "stationnumber": 1126,
    "type": "ELEVATOR"
  },
  {
    "description": "zu Gleis 7/8",
    "equipmentnumber": 10543455,
    "geocoordX": 8.6295535,
    "geocoordY": 49.87254,
    "operatorname": "DB InfraGO",
    "state": "ACTIVE",
    "stateExplanation": "available",
    "stationnumber": 1126,
    "type": "ELEVATOR"
  },
  {
    "description": "zu Gleis 9/10",
    "equipmentnumber": 10543456,
    "geocoordX": 8.6293117,
    "geocoordY": 49.8725263,
    "operatorname": "DB InfraGO",
    "state": "ACTIVE",
    "stateExplanation": "available",
    "stationnumber": 1126,
    "type": "ELEVATOR",
    "outOfService": [
      ["2024-07-18T11:00:00Z", "2024-07-18T14:00:00Z"],
      ["2024-07-19T11:00:00Z", "2024-07-19T14:00:00Z"]
    ]
  },
  {
    "description": "zu Gleis 11/12",
    "equipmentnumber": 10543457,
    "geocoordX": 8.6290451,
    "geocoordY": 49.8725147,
    "operatorname": "DB InfraGO",
    "state": "ACTIVE",
    "stateExplanation": "available",
    "stationnumber": 1126,
    "type": "ELEVATOR"
  }
]
)__"sv;

constexpr auto const kGTFS = R"(
# agency.txt
agency_id,agency_name,agency_url,agency_timezone
DB,Deutsche Bahn,https://deutschebahn.com,Europe/Berlin

# stops.txt
stop_id,stop_name,stop_lat,stop_lon,location_type,parent_station,platform_code
DA,DA Hbf,49.87260,8.63085,1,,
DA_3,DA Hbf,49.87355,8.63003,0,DA,3
DA_10,DA Hbf,49.87336,8.62926,0,DA,10
FFM,FFM Hbf,50.10701,8.66341,1,,
FFM_101,FFM Hbf,50.10773,8.66322,0,FFM,101
FFM_12,FFM Hbf,50.10658,8.66178,0,FFM,12
FFM_U,FFM Hbf,50.107577,8.6638173,0,FFM,U4
LANGEN,Langen,49.99359,8.65677,1,,1
FFM_HAUPT,FFM Hauptwache,50.11403,8.67835,1,,
FFM_HAUPT_U,Hauptwache U1/U2/U3/U8,50.11385,8.67912,0,FFM_HAUPT,
FFM_HAUPT_S,FFM Hauptwache S,50.11404,8.67824,0,FFM_HAUPT,

# routes.txt
route_id,agency_id,route_short_name,route_long_name,route_desc,route_type
S3,DB,S3,,,109
RB,DB,RB,,,106
U4,DB,U4,,,402

# trips.txt
route_id,service_id,trip_id,trip_headsign,block_id
S3,S1,S3,,
RB,S1,RB,,
U4,S1,U4,,

# stop_times.txt
trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type
S3,00:30:00,00:30:00,DA_3,0,0,0
S3,00:50:00,00:50:00,LANGEN,1,0,0
S3,01:20:00,00:20:00,FFM_HAUPT_S,2,0,0
S3,01:25:00,01:25:00,FFM_101,3,0,0
RB,00:35:00,00:35:00,DA_10,0,0,0
RB,00:45:00,00:45:00,LANGEN,1,0,0
RB,00:55:00,00:55:00,FFM_12,2,0,0
U4,01:05:00,01:05:00,FFM_U,0,0,0
U4,01:10:00,01:10:00,FFM_HAUPT_U,1,0,0

# calendar_dates.txt
service_id,date,exception_type
S1,20190501,1
)"sv;

TEST(icc, routing) {
  constexpr auto const kOsrPath = "test/test_case_osr";

  // Load OSR.
  osr::extract(true, "test/resources/test_case.osm.pbf", kOsrPath);
  auto const w = osr::ways{kOsrPath, cista::mmap::protection::READ};
  auto pl = osr::platforms{kOsrPath, cista::mmap::protection::READ};
  auto const l = osr::lookup{w};
  auto const elevator_nodes = get_elevator_nodes(w);
  auto e =
      std::make_shared<elevators>(w, elevator_nodes, parse_fasta(kFastaJson));
  pl.build_rtree(w);

  // Load timetable.
  auto tt = n::timetable{};
  tt.date_range_ = {date::sys_days{2019_y / March / 25},
                    date::sys_days{2019_y / November / 1}};
  nl::register_special_stations(tt);
  nl::gtfs::load_timetable({}, n::source_idx_t{}, nl::mem_dir::read(kGTFS), tt);
  nl::finalize(tt);

  auto const loc_rtree = create_location_rtree(tt);

  // Compute footpaths.
  auto const elevator_in_paths = compute_footpaths(tt, w, l, pl, true);

  // Match platforms.
  auto const matches = get_matches(tt, pl, w);

  // Init real-time timetable.
  auto const today = date::sys_days{2019_y / May / 1};
  auto rtt =
      std::make_shared<n::rt_timetable>(n::rt::create_rt_timetable(tt, today));

  for (auto const& [e_in_path, from_to] : elevator_in_paths) {
    auto const e_idx =
        match_elevator(e->elevators_rtree_, e->elevators_, w, e_in_path);
    if (e_idx == elevator_idx_t::invalid()) {
      std::cout << "no matching elevator found for osm_node_id="
                << w.node_to_osm_[e_in_path] << "\n";
      continue;
    }

    auto const& el = e->elevators_[e_idx];

    auto const e_nodes = l.find_elevators(geo::box{el.pos_, 1000});
    auto const e_elevators = utl::to_vec(e_nodes, [&](auto&& x) {
      return match_elevator(e->elevators_rtree_, e->elevators_, w, x);
    });
    auto const e_state_changes =
        get_state_changes(
            utl::to_vec(e_elevators,
                        [&](elevator_idx_t const ne)
                            -> std::vector<state_change<n::unixtime_t>> {
                          if (ne == elevator_idx_t::invalid()) {
                            return {
                                {.valid_from_ =
                                     n::unixtime_t{n::unixtime_t::duration{0}},
                                 .state_ = true}};
                          }
                          return e->elevators_[ne].get_state_changes();
                        }))
            .to_vec();

    auto blocked = osr::bitvec<osr::node_idx_t>{w.n_nodes()};
    for (auto const& [t, states] : e_state_changes) {
      blocked.zero_out();
      for (auto const [n, s] : utl::zip(e_nodes, states)) {
        blocked.set(n, !s);
      }

      std::cout << t << ":\n";
      for (auto const [e_node, ele_idx, state] :
           utl::zip(e_nodes, e_elevators, states)) {
        std::cout << "  osm_node_id=" << w.node_to_osm_[e_node] << ", elevator="
                  << (ele_idx != elevator_idx_t::invalid()
                          ? e->elevators_[ele_idx].desc_
                          : "INVALID")
                  << ", state=" << state << "\n";
      }

      for (auto const& [from, to] : from_to) {
        auto const p = osr::route(
            w, l, osr::search_profile::kWheelchair,
            get_loc(tt, w, pl, matches, from), get_loc(tt, w, pl, matches, to),
            3600, osr::direction::kForward, kMaxMatchingDistance, &blocked);

        std::cout << tt.locations_.names_[from].view() << " -> "
                  << tt.locations_.names_[to].view() << ": "
                  << (p.has_value() ? p->cost_
                                    : std::numeric_limits<osr::cost_t>::max())
                  << "\n";
      }
    }

    for (auto const& [from, to] : from_to) {
      std::cout << "  " << tt.locations_.names_[from].view() << " -> "
                << tt.locations_.names_[to].view() << "\n";
      for (auto const& s : el.state_changes_) {
        std::cout << "    " << s.valid_from_ << " -> " << s.state_ << "\n";
      }
    }
  }

  //  auto ioc = asio::io_context{};
  //  auto s = net::web_server{ioc};
  //  auto qr = net::query_router{}
  //                .post("/api/matches", ep::matches{loc_rtree, tt, w, l, pl})
  //                .post("/api/elevators", ep::elevators{e, w, l})
  //                .post("/api/route", ep::osr_routing{w, l, e})
  //                .post("/api/levels", ep::levels{w, l})
  //                .post("/api/platforms", ep::platforms{w, l, pl})
  //                .post("/api/graph", ep::graph{w, l})
  //                .post("/api/footpaths",
  //                      ep::footpaths{tt, w, l, pl, loc_rtree, matches, e})
  //                .post("/api/update_elevator",
  //                      ep::update_elevator{tt, w, l, pl, loc_rtree,
  //                                          elevator_nodes, matches, e, rtt})
  //                .get("/api/v1/plan",
  //                     ep::routing{w, l, pl, tt, rtt, e, loc_rtree, matches});
  //
  //  qr.serve_files("ui/build");
  //  qr.enable_cors();
  //  s.on_http_request(std::move(qr));
  //
  //  auto ec = boost::system::error_code{};
  //  s.init("0.0.0.0", "8000", ec);
  //  s.run();
  //  if (ec) {
  //    std::cerr << "error: " << ec << "\n";
  //  }
  //
  //  std::cout << "listening on 0.0.0.0:8000\n";
  //  net::run(ioc)();

  // Instantiate routing endpoint.
  auto const routing = ep::routing{w, l, pl, tt, rtt, e, loc_rtree, matches};
  auto const plan_response = routing(
      "/?fromPlace=49.87263,8.63127&toPlace=50.11347,8.67664"
      "&date=04-30-2019&time=22:00");

  std::cout << json::serialize(json::value_from(plan_response)) << "\n";
}