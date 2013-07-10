// https://github.com/philsquared/Catch/wiki/Supplying-your-own-main()
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

// test utils
#include "test_utils.hpp"

#include "vector_tile_projection.hpp"

const unsigned x=0,y=0,z=0;
const unsigned tile_size = 256;
mapnik::box2d<double> bbox;

// vector output api
#include "vector_tile_processor.hpp"
#include "vector_tile_backend_pbf.hpp"

TEST_CASE( "vector tile projection 1", "should support z/x/y to bbox conversion at 0/0/0" ) {
    mapnik::vector::spherical_mercator merc(256);
    int x = 0;
    int y = 0;
    int z = 0;
    double minx,miny,maxx,maxy;
    merc.xyz(x,y,z,minx,miny,maxx,maxy);
    mapnik::box2d<double> map_extent(minx,miny,maxx,maxy);
    mapnik::box2d<double> e(-20037508.342789,-20037508.342789,20037508.342789,20037508.342789);
    double epsilon = 0.000001;
    CHECK(std::fabs(map_extent.minx() - e.minx()) < epsilon);
    CHECK(std::fabs(map_extent.miny() - e.miny()) < epsilon);
    CHECK(std::fabs(map_extent.maxx() - e.maxx()) < epsilon);
    CHECK(std::fabs(map_extent.maxy() - e.maxy()) < epsilon);
}

TEST_CASE( "vector tile projection 2", "should support z/x/y to bbox conversion up to z33" ) {
    mapnik::vector::spherical_mercator merc(256);
    int x = 2145960701;
    int y = 1428172928;
    int z = 32;
    double minx,miny,maxx,maxy;
    merc.xyz(x,y,z,minx,miny,maxx,maxy);
    mapnik::box2d<double> map_extent(minx,miny,maxx,maxy);
    mapnik::box2d<double> e(-14210.1492817168364127,6711666.7204630710184574,-14210.1399510249066225,6711666.7297937674447894);
    double epsilon = 0.00000001;
    CHECK(std::fabs(map_extent.minx() - e.minx()) < epsilon);
    CHECK(std::fabs(map_extent.miny() - e.miny()) < epsilon);
    CHECK(std::fabs(map_extent.maxx() - e.maxx()) < epsilon);
    CHECK(std::fabs(map_extent.maxy() - e.maxy()) < epsilon);
}

TEST_CASE( "vector tile output 1", "should create vector tile with one point" ) {
    typedef mapnik::vector::backend_pbf backend_type;
    typedef mapnik::vector::processor<backend_type> renderer_type;
    typedef mapnik::vector::tile tile_type;
    tile_type tile;
    backend_type backend(tile,16);
    mapnik::Map map(tile_size,tile_size);
    mapnik::layer lyr("layer");
    lyr.set_datasource(build_ds(0,0));
    map.addLayer(lyr);
    map.zoom_to_box(bbox);
    renderer_type ren(backend,map);
    ren.apply();
    CHECK(1 == tile.layers_size());
    mapnik::vector::tile_layer const& layer = tile.layers(0);
    CHECK(std::string("layer") == layer.name());
    CHECK(1 == layer.features_size());
    mapnik::vector::tile_feature const& f = layer.features(0);
    CHECK(1 == f.id());
    CHECK(3 == f.geometry_size());
    CHECK(9 == f.geometry(0));
    CHECK(4096 == f.geometry(1));
    CHECK(4096 == f.geometry(2));
    CHECK(52 == tile.ByteSize());
    std::string buffer;
    CHECK(tile.SerializeToString(&buffer));
    CHECK(52 == buffer.size());
}

TEST_CASE( "vector tile output 2", "adding empty layers should result in empty tile" ) {
    typedef mapnik::vector::backend_pbf backend_type;
    typedef mapnik::vector::processor<backend_type> renderer_type;
    typedef mapnik::vector::tile tile_type;
    tile_type tile;
    backend_type backend(tile,16);
    mapnik::Map map(tile_size,tile_size);
    map.addLayer(mapnik::layer("layer"));
    map.zoom_to_box(bbox);
    renderer_type ren(backend,map);
    ren.apply();
    CHECK(0 == tile.layers_size());
}

TEST_CASE( "vector tile output 3", "adding layers with geometries outside rendering extent should not add layer" ) {
    typedef mapnik::vector::backend_pbf backend_type;
    typedef mapnik::vector::processor<backend_type> renderer_type;
    typedef mapnik::vector::tile tile_type;
    tile_type tile;
    backend_type backend(tile,16);
    mapnik::Map map(tile_size,tile_size);
    mapnik::layer lyr("layer");
    mapnik::context_ptr ctx = boost::make_shared<mapnik::context_type>();
    mapnik::feature_ptr feature(mapnik::feature_factory::create(ctx,1));
    mapnik::geometry_type * line = new mapnik::geometry_type(mapnik::LineString);
    line->move_to(0,0);
    line->line_to(1,1);
    feature->add_geometry(line);
    boost::shared_ptr<mapnik::memory_datasource> ds = boost::make_shared<mapnik::memory_datasource>();
    ds->push(feature);
    lyr.set_datasource(ds);
    map.addLayer(lyr);
    map.zoom_to_box(bbox);
    renderer_type ren(backend,map);
    ren.apply();
    CHECK(1 == tile.layers_size());
}

TEST_CASE( "vector tile output 4", "adding layers with degenerate geometries should not add layer" ) {
    typedef mapnik::vector::backend_pbf backend_type;
    typedef mapnik::vector::processor<backend_type> renderer_type;
    typedef mapnik::vector::tile tile_type;
    tile_type tile;
    backend_type backend(tile,16);
    mapnik::Map map(tile_size,tile_size);
    mapnik::layer lyr("layer");
    // create a datasource with a feature outside the map
    boost::shared_ptr<mapnik::memory_datasource> ds = build_ds(bbox.minx()-1,bbox.miny()-1);
    // but fake the overall envelope to ensure the layer is still processed
    // and then removed given no intersecting features will be added
    ds->set_envelope(bbox);
    lyr.set_datasource(ds);
    map.addLayer(lyr);
    map.zoom_to_box(bbox);
    renderer_type ren(backend,map);
    ren.apply();
    CHECK(0 == tile.layers_size());
}


// vector input api
#include "vector_tile_datasource.hpp"
#include "vector_tile_backend_pbf.hpp"
TEST_CASE( "vector tile input", "should be able to parse message and render point" ) {
    typedef mapnik::vector::backend_pbf backend_type;
    typedef mapnik::vector::processor<backend_type> renderer_type;
    typedef mapnik::vector::tile tile_type;
    tile_type tile;
    backend_type backend(tile,16);
    mapnik::Map map(tile_size,tile_size);
    mapnik::layer lyr("layer");
    lyr.set_datasource(build_ds(0,0));
    map.addLayer(lyr);
    map.zoom_to_box(bbox);
    renderer_type ren(backend,map);
    ren.apply();
    // serialize to message
    std::string buffer;
    CHECK(tile.SerializeToString(&buffer));
    CHECK(52 == buffer.size());
    // now create new objects
    mapnik::Map map2(tile_size,tile_size);
    tile_type tile2;
    CHECK(tile2.ParseFromString(buffer));
    CHECK(1 == tile2.layers_size());
    mapnik::vector::tile_layer const& layer2 = tile2.layers(0);
    CHECK(std::string("layer") == layer2.name());
    mapnik::layer lyr2("layer");
    boost::shared_ptr<mapnik::vector::tile_datasource> ds = boost::make_shared<
                                    mapnik::vector::tile_datasource>(
                                        layer2,x,y,z,map2.width());
    ds->set_envelope(bbox);
    lyr2.set_datasource(ds);
    lyr2.add_style("style");
    map2.addLayer(lyr2);
    mapnik::feature_type_style s;
    mapnik::rule r;
    mapnik::color red(255,0,0);
    mapnik::markers_symbolizer sym;
    sym.set_fill(red);
    r.append(sym);
    s.add_rule(r);
    map2.insert_style("style",s);
    map2.zoom_to_box(bbox);
    mapnik::image_32 im(map2.width(),map2.height());
    mapnik::agg_renderer<mapnik::image_32> ren2(map2,im);
    ren2.apply();
    unsigned rgba = im.data()(128,128);
    CHECK(red.rgba() == rgba);
    //mapnik::save_to_file(im,"test.png");
}

int main (int argc, char* const argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    // set up bbox
    double minx,miny,maxx,maxy;
    mapnik::vector::spherical_mercator merc(256);
    merc.xyz(x,y,z,minx,miny,maxx,maxy);
    bbox.init(minx,miny,maxx,maxy);
    int result = Catch::Main( argc, argv );
    if (!result) printf("\x1b[1;32m ✓ \x1b[0m\n");
    google::protobuf::ShutdownProtobufLibrary();
    return result;
}