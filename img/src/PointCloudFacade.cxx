#include "WireCellImg/PointCloudFacade.h"
#include <boost/container_hash/hash.hpp>

using namespace WireCell;
using namespace WireCell::PointCloud;
using namespace WireCell::PointCloud::Facade;
// using WireCell::PointCloud::Dataset;
using namespace WireCell::PointCloud::Tree; // for "Points" node value type
// using WireCell::PointCloud::Tree::named_pointclouds_t;

#include "WireCellUtil/Logging.h"
using spdlog::debug;

/// unused
#if 0
namespace {
    // helper to dump a dataset
    std::string dump_ds(const WireCell::PointCloud::Dataset& ds) {
        std::stringstream ss;
        for (const auto& key : ds.keys()) {;
            const auto& arr = ds.get(key);
            ss << " {" << key << ":" << arr->dtype() << ":" << arr->shape()[0] << "} ";
            // const auto& arr = ds.get(key)->elements<float>();
            // for(auto elem : arr) {
            //     ss << elem << " ";
            // }
        }
        return ss.str();
    }
    std::string dump_pcs(const ScopedBase::pointclouds_t& pcs) {
        std::stringstream ss;
        for (const auto& pc : pcs) {
            ss << dump_ds(pc) << std::endl;
        }
        return ss.str();
    }
}
#endif

std::ostream& Facade::operator<<(std::ostream& os, const Facade::Blob& blob)
{
    os << "<Blob [" << (void*)blob.hash() << "]:"
       << " npts=" << blob.npoints()
       << " r=" << blob.center_pos()
       << " q=" << blob.charge()
       << " t=[" << blob.slice_index_min() << "," << blob.slice_index_max() << "]"
       << " u=[" << blob.u_wire_index_min() << "," << blob.u_wire_index_max() << "]"
       << " v=[" << blob.v_wire_index_min() << "," << blob.v_wire_index_max() << "]"
       << " w=[" << blob.w_wire_index_min() << "," << blob.w_wire_index_max() << "]"
       << ">";
    return os;
}

Cluster* Blob::cluster()
{
    return this->m_node->parent->value.template facade<Cluster>();
}
const Cluster* Blob::cluster() const
{
    return this->m_node->parent->value.template facade<Cluster>();
}

size_t Blob::hash() const
{
    std::size_t h = 0;
    boost::hash_combine(h, npoints());
    // boost::hash_combine(h, charge());
    boost::hash_combine(h, center_x());
    boost::hash_combine(h, center_y());
    boost::hash_combine(h, center_z());
    boost::hash_combine(h, slice_index_min());
    boost::hash_combine(h, slice_index_max());
    boost::hash_combine(h, u_wire_index_min());
    boost::hash_combine(h, u_wire_index_max());
    boost::hash_combine(h, v_wire_index_min());
    boost::hash_combine(h, v_wire_index_max());
    boost::hash_combine(h, w_wire_index_min());
    boost::hash_combine(h, w_wire_index_max());
    return h;
}


void Blob::on_construct(node_type* node)
{
    this->NaryTree::Facade<points_t>::on_construct(node);

    const auto& lpcs = m_node->value.local_pcs();
    const auto& pc_scalar = lpcs.at("scalar");

    // fixme: transferring these to cache could/should be made lazy.

    // fixme: using a single array of several floats (and etc ints) would avoid
    // many single-entry arrays.

    charge_ = pc_scalar.get("charge")->elements<float_t>()[0];
    center_x_ = pc_scalar.get("center_x")->elements<float_t>()[0];
    center_y_ = pc_scalar.get("center_y")->elements<float_t>()[0];
    center_z_ = pc_scalar.get("center_z")->elements<float_t>()[0];
    npoints_ = pc_scalar.get("npoints")->elements<int_t>()[0];
    slice_index_min_ = pc_scalar.get("slice_index_min")->elements<int_t>()[0];
    slice_index_max_ = pc_scalar.get("slice_index_max")->elements<int_t>()[0];
    u_wire_index_min_ = pc_scalar.get("u_wire_index_min")->elements<int_t>()[0];
    u_wire_index_max_= pc_scalar.get("u_wire_index_max")->elements<int_t>()[0];
    v_wire_index_min_ = pc_scalar.get("v_wire_index_min")->elements<int_t>()[0];
    v_wire_index_max_ = pc_scalar.get("v_wire_index_max")->elements<int_t>()[0];
    w_wire_index_min_ = pc_scalar.get("w_wire_index_min")->elements<int_t>()[0];
    w_wire_index_max_ = pc_scalar.get("w_wire_index_max")->elements<int_t>()[0];
    ///
    ///  MAKE SURE YOU UPDATE doctest_clustering_prototype.cxx if you change the above.
    ///
}

bool Blob::overlap_fast(const Blob& b, const int offset) const
{
    if (u_wire_index_min() > b.u_wire_index_max() + offset) return false;
    if (b.u_wire_index_min() > u_wire_index_max() + offset) return false;
    if (v_wire_index_min() > b.v_wire_index_max() + offset) return false;
    if (b.v_wire_index_min() > v_wire_index_max() + offset) return false;
    if (w_wire_index_min() > b.w_wire_index_max() + offset) return false;
    if (b.w_wire_index_min() > w_wire_index_max() + offset) return false;
    return true;
}

geo_point_t Blob::center_pos() const {
    return {center_x_, center_y_, center_z_};
}


size_t Blob::nbpoints() const
{
    const auto& pc = m_node->value.local_pcs()["3d"];
    return pc.size_major();
}

bool Blob::sanity(Log::logptr_t log) const
{
    if (nbpoints() == (size_t)npoints()) return true;
    if (log) log->debug("blob sanity: blob points mismatch: {}", *this);
    return false;
}


std::vector<geo_point_t> Blob::points() const
{
    const auto& pc = m_node->value.local_pcs()["3d"];
    auto sel = pc.selection({"x","y","z"});
    const size_t npts = sel[0]->size_major();

    std::vector<geo_point_t> ret(npts);
    for (int dim=0; dim<3; ++dim) {
        auto coord = sel[dim]->elements<double>();
        for (size_t ind=0; ind<npts; ++ind) {
            ret[ind][dim] = coord[ind];
        }
    }
    return ret;
}


std::ostream& Facade::operator<<(std::ostream& os, const Facade::Cluster& cluster)
{
    os << "<Cluster [" << (void*)cluster.hash() << "]:"
       << " npts=" << cluster.npoints()
       << " nblobs=" << cluster.nchildren() << ">";
    return os;
}

Grouping* Cluster::grouping()
{
    return this->m_node->parent->value.template facade<Grouping>();
}
const Grouping* Cluster::grouping() const
{
    return this->m_node->parent->value.template facade<Grouping>();
}



const Cluster::time_blob_map_t& Cluster::time_blob_map() const
{
    if (m_time_blob_map.empty()) {
        for (const Blob* blob : children()) {
            m_time_blob_map.insert({blob->slice_index_min(), blob});
        }
    }
    return m_time_blob_map;
}


std::vector<const Blob*> Cluster::is_connected(const Cluster& c, const int offset) const
{
    std::vector<const Blob*> ret;
    for (const auto& [badtime, badblob] : c.time_blob_map()) {
        auto bad_start = badtime;
        auto bad_end = badblob->slice_index_max(); // not inclusive
        for (const auto& [good_start, goodblob] : time_blob_map()) {
            auto good_end = goodblob->slice_index_max(); // not inclusive
            if (good_end < bad_start || good_start >= bad_end) {
                continue;
            }
            if (goodblob->overlap_fast(*badblob, offset)) {
                ret.push_back(goodblob);
            }
        }
    }
    return ret;
}

const Blob* Cluster::get_first_blob() const
{
    if (time_blob_map().empty()) {
        raise<ValueError>("empty cluster has no first blob");
    }
    return time_blob_map().begin()->second;
}

const Blob* Cluster::get_last_blob() const
{
    if (time_blob_map().empty()) {
        raise<ValueError>("empty cluster has no last blob");
    }
    return time_blob_map().rbegin()->second;
}


std::pair<geo_point_t, double>
Cluster::get_closest_point_along_vec(geo_point_t& p_test1, geo_point_t dir,
                                     double test_dis, double dis_step,
                                     double angle_cut, double dis_cut) const{

    geo_point_t p_test;
  
    double min_dis = 1e9;
    double min_dis1 = 1e9;
    geo_point_t min_point = p_test1;
  
    for (int i=0; i!= int(test_dis/dis_step)+1;i++){
        p_test.set(p_test1.x() +
                   dir.x() * i * dis_step, p_test1.y() +
                   dir.y() * i * dis_step, p_test1.z() +
                   dir.z() * i * dis_step);
    
        auto pts = get_closest_point_blob(p_test);
    
        double dis = sqrt(pow(p_test.x() - pts.first.x(),2) +
                          pow(p_test.y() - pts.first.y(),2) +
                          pow(p_test.z() - pts.first.z(),2));
        double dis1 = sqrt(pow(p_test1.x() - pts.first.x(),2) +
                           pow(p_test1.y() - pts.first.y(),2) +
                           pow(p_test1.z() - pts.first.z(),2));

        if (dis < std::min(dis1 * tan(angle_cut/180.*3.1415926),dis_cut)){
            if (dis < min_dis){
                min_dis = dis;
                min_point = pts.first;
                min_dis1 = dis1;
            }
            if (dis < 3*units::cm)
                return std::make_pair(pts.first,dis1);
        }
    }

    return std::make_pair(min_point,min_dis1);
}


const Cluster::sv3d_t& Cluster::sv3d() const
{
    return m_node->value.scoped_view(scope);
}

const Cluster::kd3d_t& Cluster::kd3d() const
{
    const auto& sv = m_node->value.scoped_view(scope);
    return sv.kd();
}
geo_point_t Cluster::point3d(size_t point_index) const
{
    return kd3d().point3d(point_index);
}
const Cluster::points_type& Cluster::points() const
{
    return kd3d().points();
}
int Cluster::npoints() const
{
    if (!m_npoints) {
        const auto& sv = sv3d();
        m_npoints = sv.npoints();
    }
    return m_npoints;
}
size_t Cluster::nbpoints() const
{
    size_t ret=0;
    for (const auto* blob : children()) {
        ret += blob->nbpoints();
    }
    return ret;
}


int Cluster::nnearby(const geo_point_t& point, double radius) const
{
    auto res = kd_radius(radius, point);
    return res.size();
}


std::pair<int, int> Cluster::ndipole(const geo_point_t& point, const geo_point_t& dir) const
{
    const auto& points = this->points();
    const size_t npoints = points[0].size();

    int num_p1 = 0;
    int num_p2 = 0;

    for (size_t ind=0; ind<npoints; ++ind) {
        geo_point_t dir1(points[0][ind] - point.x(),
                         points[1][ind] - point.y(),
                         points[2][ind] - point.z());
        if (dir1.dot(dir)>=0){
            ++num_p1;
        }
        else{
            ++num_p2;
        }
    }  

    return std::make_pair(num_p1, num_p2);
}

// std::pair<int, int> Cluster::nprojection(const geo_point_t& point, const geo_point_t& dir, double dis) const
// {
//     const auto& sv = m_node->value.scoped_view(scope);       // get the kdtree
//     const auto& skd = sv.kd();
//     const auto& points = skd.points();

//     int num_p1 = 0;
//     int num_p2 = 0;

//     auto rad = skd.radius(dis*dis, point);
//     for (const auto& [index,_] : rad) {
      
//         geo_point_t dir1(points[0][index] - point.x(),
//                          points[1][index] - point.y(),
//                          points[2][index] - point.z());

//         if (dir1.dot(dir) >= 0) {
//             ++num_p1;
//         }
//         else{
//             ++num_p2;
//         }

//     }

//     return std::make_pair(num_p1, num_p2);
// }


Cluster::kd_results_t Cluster::kd_knn(int nn, const geo_point_t& query_point) const
{
    const auto& skd = kd3d();
    return skd.knn(nn, query_point);
}

Cluster::kd_results_t Cluster::kd_radius(double radius, const geo_point_t& query_point) const
{
    const auto& skd = kd3d();
    return skd.radius(radius*radius, query_point);
}

std::vector<geo_point_t> Cluster::kd_points(const Cluster::kd_results_t& res)
{
    return const_cast<const Cluster*>(this)->kd_points(res);
}
std::vector<geo_point_t> Cluster::kd_points(const Cluster::kd_results_t& res) const
{
    std::vector<geo_point_t> ret;
    const auto& points = this->points();
    for (const auto& [point_index, _] : res) {
        ret.emplace_back(
            points[0][point_index],
            points[1][point_index],
            points[2][point_index]);
    }
    return ret;
}


// can't const_cast a vector.
template<typename T>
std::vector<T*> mutify(const std::vector<const T*>& c)
{
    size_t n = c.size();
    std::vector<T*> ret(n);
    for (size_t ind=0; ind<n; ++ind) {
        ret[ind] = const_cast<T*>(c[ind]);
    }
    return ret;
}

std::vector<Blob*> Cluster::kd_blobs()
{
    return mutify(const_cast<const Cluster*>(this)->kd_blobs());
}
std::vector<const Blob*> Cluster::kd_blobs() const
{
    std::vector<const Blob*> ret;
    const auto& sv = sv3d();
    for (const auto* node : sv.nodes()) {
        ret.push_back( node->value.facade<Blob>() );
    }
    return ret;        
}

Blob* Cluster::blob_with_point(size_t point_index)
{
    return const_cast<Blob*>(
        const_cast<const Cluster*>(this)->blob_with_point(point_index));
}

const Blob* Cluster::blob_with_point(size_t point_index) const
{
    const auto& sv = sv3d();
    const auto* node = sv.node_with_point(point_index);
    return node->value.facade<Blob>();
}

std::vector<Blob*> Cluster::blobs_with_points(const kd_results_t& res)
{
    return mutify(const_cast<const Cluster*>(this)->blobs_with_points(res));
}
std::vector<const Blob*> Cluster::blobs_with_points(const kd_results_t& res) const
{
    const size_t npts = res.size();
    std::vector<const Blob*> ret(npts);
    const auto& sv = sv3d();

    for (size_t ind=0; ind<npts; ++ind) {
        const size_t point_index = res[ind].first;
        const auto* node = sv.node_with_point(point_index);
        ret[ind] = node->value.facade<Blob>();
    }
    return ret;
}


std::map<const Blob*, geo_point_t> Cluster::get_closest_blob(const geo_point_t& point, double radius) const
{
    struct Best { size_t point_index; double metric; };
    std::unordered_map<size_t, Best> best_blob_point;

    const auto& kd = kd3d();
    auto results = kd.radius(radius*radius, point);
    for (const auto& [point_index, metric] : results) {
        const size_t major_index = kd.major_index(point_index);
        auto it = best_blob_point.find(major_index);
        if (it == best_blob_point.end()) { // first time seen
            best_blob_point[major_index] = {point_index, metric};
            continue;
        }
        if (metric < it->second.metric) {
            it->second.point_index = point_index;
            it->second.metric = metric;
        }
    }
    std::map<const Blob*, geo_point_t> ret;
    for (const auto& [mi, bb] : best_blob_point) {
        ret[blob_with_point(bb.point_index)] = point3d(bb.point_index);
    }
    return ret;
}

std::pair<geo_point_t, const Blob* > Cluster::get_closest_point_blob(const geo_point_t& point) const
{
    auto results = kd_knn(1, point);
    if (results.size() == 0) {
        return std::make_pair(geo_point_t(), nullptr);
    }

    const auto& [point_index, _] = results[0];
    return std::make_pair(point3d(point_index), blob_with_point(point_index));
}

geo_point_t Cluster::calc_ave_pos(const geo_point_t& origin, const double dis) const
{
    // average position
    geo_point_t ret(0,0,0);
    double charge = 0;

    auto blob_pts = get_closest_blob(origin, dis);
    for (auto [blob, _] : blob_pts) {
        double q = blob->charge();
        if (q==0) q=1;
        ret += blob->center_pos()*q;
        charge += q;
    }
    
    if (charge != 0) {
        ret = ret / charge;
    }

    return ret;
}

#include <boost/histogram.hpp>
#include <boost/histogram/algorithm/sum.hpp>
namespace bh = boost::histogram;
namespace bha = boost::histogram::algorithm;


// Example parameter calculating functions used by directional hough
// transforms.
static
double theta_angle(const Vector& dir) {
    const Vector Z(0,0,1);
    return acos(Z.dot(dir)); 
}
static
double theta_cosine(const Vector& dir) { 
    const Vector Z(0,0,1);
    return Z.dot(dir); 
}
static
double phi_angle(const Vector& dir) {
    const Vector X(1,0,0);
    const Vector Y(0,1,0);
    return atan2(Y.dot(dir), X.dot(dir));
}




std::pair<double, double>
Cluster::hough_transform(const geo_point_t& origin, const double dis,
                         HoughParamSpace param_space) const
{
    // auto results = skd.radius(dis*dis, origin);
    auto results = kd_radius(dis, origin);

    if(results.size() == 0) {
        return {0,0};
    }
    constexpr double pi = 3.141592653589793;

    using direction_parameter_function_f = std::function<double(const Vector& dir)>;

    // Parameter axis 1 is some measure of theta angle (angle or cosine)
    const int nbins1 = 180;
    // param_space == costh_phi
    direction_parameter_function_f theta_param = theta_cosine;
    double min1=-1.0, max1=1.0;
    if ( param_space == HoughParamSpace::theta_phi ) {
        theta_param = theta_angle;
        min1 = 0;
        max1 = pi;
    }

    // Parameter axis 2 is only supported by phi angle
    const int nbins2 = 360;
    const double min2 = -pi;
    const double max2 = +pi;
    direction_parameter_function_f phi_param = phi_angle;

    auto hist = bh::make_histogram(bh::axis::regular<>( nbins1, min1, max1 ),
                                   bh::axis::regular<>( nbins2, min2, max2 ) );

    const auto& blobs = blobs_with_points(results);
    const auto pts = kd_points(results);

    for (size_t ind=0; ind<blobs.size(); ++ind) {
        const auto* blob = blobs[ind];
        auto charge = blob->charge();
        // protection against the charge=0 case ...
        if (charge == 0) charge = 1;
        if (charge <= 0) continue;
            
        const auto npoints = blob->npoints();
        const auto& pt = pts[ind];

        const Vector dir = (pt-origin).norm();

        const double p1 = theta_param(dir);
        const double p2 = phi_param(dir);
        hist(p1, p2, bh::weight(charge/npoints));
    }
        
    auto indexed = bh::indexed(hist);
    auto it = std::max_element(indexed.begin(), indexed.end());
    const auto& cell = *it;
    return {cell.bin(0).center(), cell.bin(1).center()};
}


geo_point_t Cluster::vhough_transform(const geo_point_t& origin, const double dis,
                                      HoughParamSpace param_space) const
{
    if ( param_space == HoughParamSpace::theta_phi ) {
        const auto [th, phi] = hough_transform(origin, dis, param_space);
        return {sin(th)*cos(phi), sin(th)*sin(phi), cos(th)};
    }
    // costh_phi
    const auto [cth, phi] = hough_transform(origin, dis, param_space);
    const double sth = sqrt(1-cth*cth);
    return {sth*cos(phi), sth*sin(phi), cth};
}


std::tuple<int, int, int, int> Cluster::get_uvwt_min() const
{
    std::tuple<int, int, int, int> ret;
    bool first = true;
    
    for (const auto* blob : children()) {
        const int u = blob->u_wire_index_min();
        const int v = blob->v_wire_index_min();
        const int w = blob->w_wire_index_min();
        const int t = blob->slice_index_min();

        if (first) {
            ret = {u,v,w,t};
            continue;
        }
        get<0>(ret) = std::min(get<0>(ret), u);
        get<1>(ret) = std::min(get<1>(ret), v);
        get<2>(ret) = std::min(get<2>(ret), w);
        get<3>(ret) = std::min(get<3>(ret), t);
    }
    return ret;
}
std::tuple<int, int, int, int> Cluster::get_uvwt_max() const
{
    std::tuple<int, int, int, int> ret;
    bool first = true;
    
    for (const auto* blob : children()) {
        const int u = blob->u_wire_index_max();
        const int v = blob->v_wire_index_max();
        const int w = blob->w_wire_index_max();
        const int t = blob->slice_index_max();

        if (first) {
            ret = {u,v,w,t};
            continue;
        }
        get<0>(ret) = std::max(get<0>(ret), u);
        get<1>(ret) = std::max(get<1>(ret), v);
        get<2>(ret) = std::max(get<2>(ret), w);
        get<3>(ret) = std::max(get<3>(ret), t);
    }
    return ret;
}

// FIXME: Is this actually correct?  It does not return "ranges" but rather the
// number of unique wires/ticks in the cluster.  A sparse but large cluster will
// be "smaller" than a small but dense cluster.
std::tuple<int, int, int, int> Cluster::get_uvwt_range() const
{
    std::set<int> u_set;
    std::set<int> v_set;
    std::set<int> w_set;
    std::set<int> t_set;
    for (const auto* blob : children()) {
        for(int i = blob->u_wire_index_min(); i < blob->u_wire_index_max(); ++i) {
            u_set.insert(i);
        }
        for(int i = blob->v_wire_index_min(); i < blob->v_wire_index_max(); ++i) {
            v_set.insert(i);
        }
        for(int i = blob->w_wire_index_min(); i < blob->w_wire_index_max(); ++i) {
            w_set.insert(i);
        }
        for(int i = blob->slice_index_min(); i < blob->slice_index_max(); ++i) {
            t_set.insert(i);
        }
    }
    return {u_set.size(), v_set.size(), w_set.size(), t_set.size()};
}


double Cluster::get_length() const
{
    if (m_length == 0) {        // invalidates when a new node is set
        const auto& tp = grouping()->get_params();

        const auto [u, v, w, t] = get_uvwt_range();
        const double pu = u*tp.pitch_u;
        const double pv = v*tp.pitch_v;
        const double pw = w*tp.pitch_w;
        const double pt = t*tp.tick_drift;
        m_length = std::sqrt(2./3.*(pu*pu + pv*pv + pw*pw) + pt*pt);
    }
    return m_length;
}


std::pair<geo_point_t, geo_point_t> Cluster::get_highest_lowest_points(size_t axis) const
{
    const auto& points = this->points();
    const size_t npoints = points[0].size();

    geo_point_t lowest_point, highest_point;

    for (size_t ind=0; ind<npoints; ++ind) {
        geo_point_t pt(points[0][ind],
                       points[1][ind],
                       points[2][ind]);
        if (!ind) {
            lowest_point = highest_point = pt;
            continue;
        }
        if (pt[axis] > highest_point[axis]) {
            highest_point = pt;
        }
        if (pt[axis] < lowest_point[axis]) {
            lowest_point = pt;
        }
    }

    return std::make_pair(highest_point, lowest_point);
}


std::pair<geo_point_t, geo_point_t> Cluster::get_earliest_latest_points() const
{
    auto backwards = get_highest_lowest_points(0);
    return std::make_pair(backwards.second, backwards.first);
}


std::ostream& Facade::operator<<(std::ostream& os, const Facade::Grouping& grouping)
{
    os << "<Grouping ["<<(void*)&grouping<<"]:"
       << " nclusters=" << grouping.nchildren() << ">";
    return os;
}


std::string Facade::dump(const Facade::Grouping& grouping, int level)
{
    std::stringstream ss;

    ss << grouping;
    if (level == 0) {
        return ss.str();
    }
    ss << "\n";
    size_t nc=0;
    for (const auto* cluster : grouping.children()) {
        ss << nc++ << "\t" << *cluster << "\n";
        if (level == 1) {
            continue;
        }
        size_t nb = 0;
        for (const auto* blob : cluster->children()) {
            ss << nb++ << "\t\t" << *blob << "\n";
        }
    }
    return ss.str();
}


bool Cluster::sanity(Log::logptr_t log) const
{
    {
        const auto* svptr = m_node->value.get_scoped(scope);
        if (!svptr) {
            if (log) log->debug("cluster sanity: note, not yet a scoped view {}", scope);
        }
    }
    if (!nchildren()) {
        if (log) log->debug("cluster sanity: no children blobs");
        return false;
    }

    const auto& sv = m_node->value.scoped_view(scope);
    const auto& snodes = sv.nodes();
    if (snodes.empty()) {
        if (log) log->debug("cluster sanity: no scoped nodes");
        return false;
    }
    if (sv.npoints() == 0) {    // triggers a scoped view cache fill
        if (log) log->debug("cluster sanity: no scoped points");
        return false;
    }
    // sv.force_invalid();
    const auto& skd = sv.kd();

    const auto& fblobs = children();

    for (const Blob* blob : fblobs) {
        if (!blob->sanity(log)) return false;
    }

    if (skd.nblocks() != snodes.size()) {
        if (log) log->debug("cluster sanity: k-d blocks={} scoped nodes={}", skd.nblocks(), fblobs.size());
        return false;
    }

    if (skd.nblocks() != fblobs.size()) {
        if (log) log->debug("cluster sanity: k-d blocks={} cluster blobs={}", skd.nblocks(), fblobs.size());
        return false;
    }

    for (size_t ind=0; ind<snodes.size(); ++ind) {
        /// In general, the depth-first order of scoped view nodes is always the
        /// same as k-d tree blocks but is not (again in general) expected to be
        /// the same order as the children blobs of a parent cluster.  After
        /// all, a scoped view may span multiple essentially any subset of tree
        /// nodes.  However, in the special case of the "3d" SV and how the PC
        /// tree is constructed, the depth-first and children blobs ordering
        /// should be "accidentally" the same.
        const auto* fblob = fblobs[ind];
        const auto* sblob = snodes[ind]->value.facade<Blob>();
        if (fblob != sblob) {
            if (log) {
                log->debug("cluster sanity: scoped node facade Blob differs from cluster child at {}", ind);
                log->debug("cluster sanity: \tscoped blob: {}", *fblob);
                log->debug("cluster sanity: \tfacade blob: {}", *sblob);                
            }
            // return false;
        }
    }

    const auto& majs = skd.major_indices();
    const auto& mins = skd.minor_indices();
    const size_t npts = skd.npoints();

    const Blob* sblob = nullptr;
    std::vector<geo_point_t> spoints;

    for (size_t ind=0; ind<npts; ++ind) {
        auto kdpt = skd.point3d(ind);

        const size_t majind = majs[ind];
        const size_t minind = mins[ind];
        
        // scoped consistency
        const node_t* tnode = sv.node_with_point(ind);
        if (!tnode) {
            if (log) log->debug("cluster sanity: scoped node facade not a Blob at majind={}", majind);
            return false;
        }
        const auto* tblob = tnode->value.facade<Blob>();
        if (tblob != sblob) {
            sblob = tblob;
            spoints = sblob->points();
        }

        if (minind >= spoints.size()) {
            if (log) log->debug("cluster sanity: minind={} is beyond scoped blob npts={} majind={}, blob is: {}",
                                minind, spoints.size(), majind, *sblob);
            return false;
        }
        auto spt = spoints[minind];
        if (spt != kdpt) {
            if (log) log->debug("cluster sanity: scoped point mismatch at minind={} majind={} spt={} kdpt={}, blob is: {}",
                                minind, majind, spt, kdpt, *sblob);
            return false;
        }

    }
    return true;
}



size_t Cluster::hash() const
{
    std::size_t h = 0;
    boost::hash_combine(h, (size_t)(get_length()/units::mm));
    auto blobs = children();    // copy vector
    sort_blobs(blobs);
    for (const Blob* blob : blobs) {
        boost::hash_combine(h, blob->hash());
    }
    return h;
}

size_t Grouping::hash() const
{
    std::size_t h = 0;
    boost::hash_combine(h,m_tp.pitch_u);
    boost::hash_combine(h,m_tp.pitch_v);
    boost::hash_combine(h,m_tp.pitch_w);
    boost::hash_combine(h,m_tp.angle_u);
    boost::hash_combine(h,m_tp.angle_v);
    boost::hash_combine(h,m_tp.angle_w);
    boost::hash_combine(h,m_tp.tick_drift);
    auto clusters = children(); // copy vector
    sort_clusters(clusters);
    for (const Cluster* cluster : clusters) {
        boost::hash_combine(h, cluster->hash());
    }
    return h;
}

bool Facade::blob_less(const Facade::Blob* a, const Facade::Blob* b)
{
    if (a==b) return false;
    {
        const auto na = a->npoints();
        const auto nb = b->npoints();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        const auto na = a->charge();
        const auto nb = b->charge();
        if (na < nb) return true;
        if (nb < na) return false;
    }

    {
        const auto na = a->slice_index_min();
        const auto nb = b->slice_index_min();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        const auto na = a->slice_index_max();
        const auto nb = b->slice_index_max();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        const auto na = a->u_wire_index_min();
        const auto nb = b->u_wire_index_min();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        const auto na = a->v_wire_index_min();
        const auto nb = b->v_wire_index_min();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        const auto na = a->w_wire_index_min();
        const auto nb = b->w_wire_index_min();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        const auto na = a->u_wire_index_max();
        const auto nb = b->u_wire_index_max();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        const auto na = a->v_wire_index_max();
        const auto nb = b->v_wire_index_max();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        const auto na = a->w_wire_index_max();
        const auto nb = b->w_wire_index_max();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    return a<b;
}
void Facade::sort_blobs(std::vector<const Blob*>& blobs)
{
    std::sort(blobs.rbegin(), blobs.rend(), blob_less);
}
void Facade::sort_blobs(std::vector<Blob*>& blobs)
{
    std::sort(blobs.rbegin(), blobs.rend(), blob_less);
}


bool Facade::cluster_less(const Cluster* a, const Cluster* b)
{
    if (a==b) return false;

    {
        const double la = a->get_length();
        const double lb = b->get_length();
        if (la < lb) return true;
        if (lb < la) return false;
    }
    {
        const int na = a->nchildren();
        const int nb = b->nchildren();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        const int na = a->npoints();
        const int nb = b->npoints();
        if (na < nb) return true;
        if (nb < na) return false;
    }
    {
        auto ar = a->get_uvwt_min();
        auto br = b->get_uvwt_min();
        if (get<0>(ar) < get<0>(br)) return true;
        if (get<0>(br) < get<0>(ar)) return false;
        if (get<1>(ar) < get<1>(br)) return true;
        if (get<1>(br) < get<1>(ar)) return false;
        if (get<2>(ar) < get<2>(br)) return true;
        if (get<2>(br) < get<2>(ar)) return false;
        if (get<3>(ar) < get<3>(br)) return true;
        if (get<3>(br) < get<3>(ar)) return false;
    }
    {
        auto ar = a->get_uvwt_max();
        auto br = b->get_uvwt_max();
        if (get<0>(ar) < get<0>(br)) return true;
        if (get<0>(br) < get<0>(ar)) return false;
        if (get<1>(ar) < get<1>(br)) return true;
        if (get<1>(br) < get<1>(ar)) return false;
        if (get<2>(ar) < get<2>(br)) return true;
        if (get<2>(br) < get<2>(ar)) return false;
        if (get<3>(ar) < get<3>(br)) return true;
        if (get<3>(br) < get<3>(ar)) return false;
    }

    // The two are very similar.  What is left to check?  Only pointer?.  This
    // will cause "randomness"!
    return a < b;           
}
void Facade::sort_clusters(std::vector<const Cluster*>& clusters)
{
    std::sort(clusters.rbegin(), clusters.rend(), cluster_less);
}
void Facade::sort_clusters(std::vector<Cluster*>& clusters)
{
    std::sort(clusters.rbegin(), clusters.rend(), cluster_less);
}






// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:
