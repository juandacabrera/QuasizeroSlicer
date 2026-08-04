// Quasizero Slicer - segment subdivider tests. GNU AGPLv3.
#include "standalone/qz_test.hpp"
#include "libslic3r/QuasiZero/QzShortSegmentAnchor.hpp"
#include <sstream>

using namespace Slic3r::QuasiZero;

namespace {
size_t count_of(const std::string &s, const std::string &n)
{ size_t c=0,p=0; while ((p=s.find(n,p))!=std::string::npos){++c;p+=n.size();} return c; }
double sum_rel_e(const std::string &g)
{
    double sum=0; std::istringstream is(g); std::string l;
    while (std::getline(is,l)) { auto p=l.find(" E"); if(p!=std::string::npos && l.rfind("G1",0)==0) sum+=std::atof(l.c_str()+p+2); }
    return sum;
}
}

QZ_TEST(subdiv_splits_long_moves_preserving_total_e_relative)
{
    QzSegmentSubdivider sub(2.0, true);
    const std::string out = sub.process(
        "G1 X0 Y0 F3600\n"
        "G1 X60 Y0 E3.0 F600\n");
    QZ_CHECK(count_of(out, "G1 X") == 31);                 // 1 travel + 30 pieces
    QZ_CHECK_NEAR(sum_rel_e(out), 3.0, 1e-4);              // total E preserved
    QZ_CHECK(out.find("X60.000 Y0.000") != std::string::npos); // exact endpoint
    QZ_CHECK(count_of(out, "F600") == 1);                  // feedrate once
}

QZ_TEST(subdiv_absolute_e_ends_exact)
{
    QzSegmentSubdivider sub(2.0, false);
    const std::string out = sub.process(
        "M82\nG92 E0\n"
        "G1 X0 Y0 F3600\n"
        "G1 X10 Y0 E5.0\n");
    QZ_CHECK(count_of(out, "G1 X") == 6);                  // travel + 5 pieces
    QZ_CHECK(out.find("E5.00000") != std::string::npos);   // exact final E
}

QZ_TEST(subdiv_leaves_short_moves_and_travels_alone)
{
    QzSegmentSubdivider sub(2.0, true);
    const std::string in =
        "G1 X0 Y0 F3600\n"
        "G1 X1.5 Y0 E0.1\n"
        "G1 X50 Y0 F3600\n";
    QZ_CHECK(sub.process(in) == in);
}
