#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <list>
#include <vector>
#include <random>
#include <array>
#include <execution>


using Real = double;
using Vector2D = std::array< Real, 2 >;
using Vector3D = std::array< Real, 3 >;
using Color = std::array< unsigned char, 3 >;

struct Scene
{
    unsigned width;
    unsigned height;
    unsigned maxDepth;
    unsigned rndDepthRate; // in percent
    unsigned rndColorRate; // in percent
    Real plateWidth;
    unsigned textureWidth;
    Real materialRetransmission;
    Real wavelengthDecayDistance;
    Vector3D wavelengthDecay;
};

using DepthsMap = std::vector< std::vector< int > >;

DepthsMap GenerateDepthsMap( const Scene & _scene )
{
    auto rows{ _scene.height };
    auto depth{ static_cast< int >( _scene.maxDepth ) };
    //std::random_device rndDevice;
    //std::mt19937 rnd{ rndDevice() }; // TODO
    std::mt19937 rnd{ 4 };
    std::uniform_int_distribution< int > rndDepthTrigger( 0, 100 );
    auto rndDepthRate{ 100 - static_cast< int >( _scene.rndDepthRate ) };
    std::uniform_int_distribution< int > rndDepth( -depth, depth );
    DepthsMap depthsMap;
    while( rows-- != 0 ) {
        auto & colsMap{ depthsMap.emplace_back( std::vector< int >{} ) };
        auto cols{ _scene.width };
        while( cols-- != 0 )
            colsMap.emplace_back( rndDepthTrigger( rnd ) >= rndDepthRate ? rndDepth( rnd ) : 0 );
    }       
    return depthsMap;
}

struct Plate;

struct Photon
{
    Vector3D position;
    Vector3D normal;
    Vector3D color;
};

struct Material
{
    Vector3D color;
    Real retransmission;
};

struct Plate
{
    std::array< Vector3D, 4 > positions;
    std::array< Vector2D, 4 > textures;
    Vector3D normal;
    Material material;
    std::vector< Photon > emitters;
    std::vector< Vector3D > receivers;
    std::vector< Color > colors;
};

using Plates = std::vector< Plate >;


Vector3D GetColor( std::mt19937 & _rnd, std::uniform_int_distribution< int > & _rndColorTrigger, const int _rndColorRate )
{
    if( _rndColorTrigger( _rnd ) < _rndColorRate )
        return { 1, 1, 1 }; // white
    const auto rndColor{ _rndColorTrigger( _rnd ) };
    if( rndColor > 66 )
        return { 1, 0.5, 0.5 }; // redish
    if( rndColor > 33 )
        return { 0.5, 1, 0.5 }; // greenish
    return { 0.5, 0.5, 1 }; // blueish
}


Plates GeneratePlates( const Scene & _scene, const DepthsMap & _depthsMap )
{
    //std::random_device rndDevice;
    //std::mt19937 rnd{ rndDevice() }; // TODO
    std::mt19937 rnd{ 20 };
    std::uniform_int_distribution< int > rndColorTrigger( 0, 100 );
    auto rndColorRate{ 100 - static_cast< int >( _scene.rndColorRate ) };
    Plates plates;
    const auto halfWidth{ static_cast< Real >( _scene.width ) * _scene.plateWidth * Real{ 0.5 } };
    const auto halfHeight{ static_cast< Real >( _scene.height ) * _scene.plateWidth * Real{ 0.5 } };
    for( unsigned row{ 0 }; row < _scene.height; ++row ) {
        const auto & colsMap{ _depthsMap[ row ] };
        const auto rowTop{ static_cast< Real >( ( row + 0 ) ) * _scene.plateWidth - halfHeight };
        const auto rowBtm{ static_cast< Real >( ( row + 1 ) ) * _scene.plateWidth - halfHeight };
        for( unsigned col{ 0 }; col < _scene.width; ++col ) {
            const int depth{ colsMap[ col ] };
            const auto currDepth{ static_cast< Real >( depth ) * _scene.plateWidth };
            const auto colLft{ static_cast< Real >( ( col + 0 ) ) * _scene.plateWidth - halfWidth };
            const auto colRgt{ static_cast< Real >( ( col + 1 ) ) * _scene.plateWidth - halfWidth };
            plates.emplace_back( Plate{
                { Vector3D{ colLft, rowTop, currDepth }, { colRgt, rowTop, currDepth },
                         { colRgt, rowBtm, currDepth }, { colLft, rowBtm, currDepth } },
                { Vector2D{ 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } }, Vector3D{ 0, 0, -1 },
                Material{ { 1, 1, 1 }, _scene.materialRetransmission } } );
            if( col < _scene.width - 1 ) {
                const int depth2{ colsMap[ col + 1 ] };
                const int depthInc{ depth2 > depth ? 1 : -1 };
                for( auto d{ depth }; d != depth2; d += depthInc ) {
                    const auto currDepth1{ static_cast< Real >( d ) * _scene.plateWidth };
                    const auto currDepth2{ static_cast< Real >( d + depthInc ) * _scene.plateWidth };
                    const auto normalU{ currDepth1 > currDepth2 ? Real{ -1 } : Real{ 1 } };
                    plates.emplace_back( Plate{
                        { Vector3D{ colRgt, rowTop, currDepth1 }, { colRgt, rowTop, currDepth2 },
                                 { colRgt, rowBtm, currDepth2 }, { colRgt, rowBtm, currDepth1 } },
                        { Vector2D{ 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } }, Vector3D{ normalU, 0, 0 },
                        Material{ GetColor( rnd, rndColorTrigger, rndColorRate ), _scene.materialRetransmission } } );
                }
            }
            if( row < _scene.height - 1 ) {
                const int depth2{ _depthsMap[ row + 1 ][ col ] };
                const int depthInc{ depth2 > depth ? 1 : -1 };
                for( auto d{ depth }; d != depth2; d += depthInc ) {
                    const auto currDepth1{ static_cast< Real >( d ) * _scene.plateWidth };
                    const auto currDepth2{ static_cast< Real >( d + depthInc ) * _scene.plateWidth };
                    const auto normalV{ currDepth1 > currDepth2 ? Real{ -1 } : Real{ 1 } };
                    plates.emplace_back( Plate{
                        { Vector3D{ colLft, rowBtm, currDepth1 }, { colRgt, rowBtm, currDepth1 },
                                 { colRgt, rowBtm, currDepth2 }, { colLft, rowBtm, currDepth2 } },
                        { Vector2D{ 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } }, Vector3D{ 0, normalV, 0 },
                        Material{ GetColor( rnd, rndColorTrigger, rndColorRate ), _scene.materialRetransmission } } );
                }
            }
        }
    }
    return plates;
}


inline static Vector3D VecSub( const Vector3D & _a, const Vector3D & _b )
{
    return { _a[ 0 ] - _b[ 0 ], _a[ 1 ] - _b[ 1 ], _a[ 2 ] - _b[ 2 ] };
}

inline static Vector3D VecMult( const Vector3D & _a, const Real & _b )
{
    return { _a[ 0 ] * _b, _a[ 1 ]  * _b, _a[ 2 ]  * _b };
}

inline static Vector3D VecMult( const Vector3D & _a, const Vector3D & _b )
{
    return { _a[ 0 ] * _b[ 0 ], _a[ 1 ]  * _b[ 1 ], _a[ 2 ]  * _b[ 2 ] };
}

inline static Vector3D VecAdd( const Vector3D & _a, const Vector3D & _b )
{
    return { _a[ 0 ] + _b[ 0 ], _a[ 1 ] + _b[ 1 ], _a[ 2 ] + _b[ 2 ] };
}

inline static Vector3D VecCross( const Vector3D & _a, const Vector3D & _b )
{
    return { _a[ 1 ] * _b[ 2 ] - _a[ 2 ] * _b[ 1 ], _a[ 2 ] * _b[ 0 ] - _a[ 0 ] * _b[ 2 ], _a[ 0 ] * _b[ 1 ] - _a[ 1 ] * _b[ 0 ] };
}

inline static Real VecDot( const Vector3D & _a, const Vector3D & _b )
{
    return _a[ 0 ] * _b[ 0 ] + _a[ 1 ] * _b[ 1 ] + _a[ 2 ] * _b[ 2 ];
}

inline static Real VecDist( const Vector3D & _a, const Vector3D & _b )
{
    const auto sub{ VecSub( _b, _a ) };
    return std::sqrt( std::pow( sub[ 0 ], 2 ) + std::pow( sub[ 1 ], 2 ) + std::pow( sub[ 2 ], 2 ) );
}

inline static Vector3D VecNorm( const Vector3D & _a )
{
    return VecMult( _a, Real{ 1 } / VecDist( { 0, 0, 0 }, _a ) );
}


inline static const Real epsilon{ std::numeric_limits< Real >::min() };

inline static bool SegmentIntersectsTriangle( const Vector3D & _p0, const Vector3D & _p1,
    const Vector3D & _a, const Vector3D & _b, const Vector3D & _c )
{
    const auto dir{ VecSub( _p1, _p0 ) };
    const auto edge1{ VecSub( _b, _a ) };
    const auto edge2{ VecSub( _c, _a ) };
    const auto h{ VecCross( dir, edge2 ) };
    const auto a{ VecDot( edge1, h ) };
    if( a < epsilon && a > -epsilon )
        return false;
    const auto f{ Real{ 1 } / a };
    const auto s{ VecSub( _p0, _a ) };
    const auto u{ f * VecDot( s, h ) };
    if( u < Real{ 0 } || u > Real{ 1 } )
        return false;
    const auto q{ VecCross( s, edge1 ) };
    const auto v{ f * VecDot( dir, q ) };
    if( v < Real{ 0 } || ( u + v ) > Real{ 1 } )
        return false;
    const auto t{ f * VecDot( edge2, q ) };
    if( t > epsilon && t < Real{ 1 } + epsilon )
        return true;
    return false;
}

inline static bool VecInPlane( const Vector3D & _p, const Vector3D & _a, const Vector3D & _b, const Vector3D & _c )
{
    const auto normal{ VecCross( VecSub( _b, _a ), VecSub( _c, _a ) ) };
    return std::abs( VecDot( normal, _p ) - VecDot( normal, _a ) ) < epsilon;
}

inline static bool VecFacing( const Vector3D & _a, const Vector3D & _b )
{
    return VecDot( _a, _b ) <= 0;
}


int main( int _argc, char * _argv[] )
{
    std::cout << "[lightshot] a CPU-based photon tracer" << std::endl << std::endl;
    unsigned defaultResolution{ 16 };
    std::cout << "usage: Lightshot resolution (must be a power of two, default is " << defaultResolution << ")" << std::endl;

    unsigned resolution{ 0 };
    if( _argc == 2 )
        resolution = std::atoi( _argv[ 1 ] );
    const bool resolutionIsPowerOfTwo{ ( resolution > 0 ) && ( ( resolution & ( resolution - 1 ) ) == 0 ) };
    if( resolution == 0 || !resolutionIsPowerOfTwo ) {
        std::cout << "> no parameter, bad parameter, or parameter value is not a power of two: using default." << std::endl << std::endl;
        resolution = defaultResolution;
    }
    
    std::cout << "press 'space' key to toggle between linear/nearest texture filter" << std::endl;
    std::cout << "press 'esc' key to exit" << std::endl << std::endl;

    const Scene scene{
        7, 7, // scene dimensions
        2, // scene depth
        25, // depth rate
        30, // color rate
        1, // plate width
        resolution, // texture width
        0.5, // material energy retransmission ratio
        10, // wavelength decay distance
        { 0.8, 0.9, 1 }, // wavelength decay
    };

    static constexpr unsigned maxRenderingPass{ 4 }; // maximum rendering pass
                
    static constexpr Real realMultiplier{ 1000 }; // increase precision on real cumulation operations
    static const auto lightResolution{ Real{ 1 } / std::pow( scene.textureWidth, 2 ) };
    bool nearestTextureRendering{ false }; // nearest or linear interpolation

    // generate depths map and related plates:
    auto plates{ GeneratePlates( scene, GenerateDepthsMap( scene ) ) };

    std::cout << plates.size() << " plates, resolution: " << scene.textureWidth << "x" << scene.textureWidth << std::endl;
    std::cout << "material light retransmission rate: " << static_cast< int >( scene.materialRetransmission * 100 ) << "%" << std::endl;
    
    // prepare plates:
    std::for_each( std::execution::par_unseq, plates.begin(), plates.end(), [ & ]( auto & _plate ) {
        
        // init and reset receiver buffer:
        _plate.receivers.resize( scene.textureWidth * scene.textureWidth );
        std::memset( _plate.receivers.data(), 0, scene.textureWidth * scene.textureWidth * sizeof( Vector3D ) );

        // init emitter buffer:
        _plate.emitters.resize( scene.textureWidth * scene.textureWidth );
        auto itEmitter{ _plate.emitters.begin() };

        // precompute positions of each texture point in 3D:
        const auto & a{ _plate.positions[ 0 ] };
        const auto ba{ VecSub( _plate.positions[ 1 ], a ) };
        const auto da{ VecSub( _plate.positions[ 3 ], a ) };
        for( unsigned int y{ 0 }; y < scene.textureWidth; ++y ) {
            const auto daY{ VecMult( da, ( static_cast< Real >( y ) + Real{ 0.5 } ) / scene.textureWidth ) };
            for( unsigned int x{ 0 }; x < scene.textureWidth; ++x, ++itEmitter ) {
                const auto baX{ VecMult( ba, ( static_cast< Real >( x ) + Real{ 0.5 } ) / scene.textureWidth ) };
                itEmitter->position = VecAdd( VecAdd( a, baX ), daY ); // save position in emitter
                itEmitter->color = Vector3D{ 0, 0, 0 }; // reset emitter color
            }
        }
    } );

    // light sources:
    const std::vector< Photon > lightSources{
        { { 1.01, 3.23, -2.34 }, { 0, 0, 1 }, VecMult( Vector3D{ 1, 0.95, 0.9 }, Real{ 0.2 } / Real{ maxRenderingPass } ) }
    };

    // initial photons list:
    std::vector< Photon > photons;
    const auto textureWidth{ static_cast< Real >( scene.textureWidth ) };
    const auto halfPlateWidth{ scene.plateWidth / 2 };
    for( const auto & lightSource : lightSources ) {
        const auto & position{ lightSource.position };
        const auto color{ VecMult( lightSource.color, realMultiplier * lightResolution ) };
        for( unsigned y{ 0 }; y < scene.textureWidth; ++y ) {
            const auto shiftY{ ( static_cast< Real >( y ) * scene.plateWidth / textureWidth ) - halfPlateWidth };
            for( unsigned x{ 0 }; x < scene.textureWidth; ++x ) {
                const auto shiftX{ ( static_cast< Real >( x ) * scene.plateWidth / textureWidth ) - halfPlateWidth };
                photons.emplace_back( Photon{ VecAdd( VecMult( position, scene.plateWidth ), Vector3D{ shiftX, shiftY, 0 } ), lightSource.normal, color } );
            }
        }
    }

    // progress bar:
    unsigned plateCompleted{ 0 };
    const auto Progress{ [ & ]{
            static const auto Draw{ []( const unsigned _length, const char _char ){ for( unsigned i{ 0 }; i < _length; ++i ) std::cout << _char; } };
            static std::mutex mtx;
            std::lock_guard lock{ mtx };
            const unsigned percent{ plateCompleted * 100 / static_cast< unsigned >( plates.size() ) };
            constexpr unsigned length{ 50 };
            constexpr unsigned lengthMargin{ length + 7 };
            Draw( lengthMargin, '\r' );
            const unsigned barLength{ plateCompleted * length / static_cast< unsigned >( plates.size() ) };
            std::cout << "[";
            Draw( barLength, 'O' );
            Draw( length - barLength, ' ' );
            std::cout << "] " << percent << "%";
            if( ++plateCompleted != plates.size() )
                return;
            Draw( lengthMargin, '\r' ); Draw( lengthMargin, ' ' ); Draw( lengthMargin, '\r' );
        } };

    const auto t0{ std::chrono::high_resolution_clock::now() };

    // rendering pass for each photo list:
    unsigned renderingPass{ 0 };
    while( !photons.empty() && renderingPass++ != maxRenderingPass ) {
        std::cout << "pass " << renderingPass << "/" << maxRenderingPass << " - " << photons.size() << " photon(s)" << std::endl;

        // for each plate:
        plateCompleted = 0;
        std::for_each( std::execution::par_unseq, plates.begin(), plates.end(), [ & ]( auto & _plate ) {

            // for each photon:
            for( const auto & photon : photons ) {
                
                // for each texture point:
                auto itEmitter{ _plate.emitters.begin() };
                for( auto itReceiver{ _plate.receivers.begin() }; itReceiver != _plate.receivers.cend(); ++itReceiver, ++itEmitter ) {
                    auto & receiver{ *itReceiver };
                    auto & emitter{ *itEmitter };
                    const auto & position{ emitter.position };

                    const auto rayNormal{ VecNorm( VecSub( photon.position, position ) ) };
                    if( !VecFacing( rayNormal, photon.normal ) )
                        continue; // the normal of the ray is not facing the photon ray

                    // intersections with other plates:
                    bool intersect{ false };
                    for( int j{ 0 }; !intersect && j < plates.size(); ++j ) {
                        const auto & plate2{ plates[ j ] };
                        if( &_plate == &plate2 )
                            continue;
                        const auto & a2{ plate2.positions[ 0 ] };
                        const auto & c2{ plate2.positions[ 2 ] };
                        intersect = SegmentIntersectsTriangle( position, photon.position, a2, plate2.positions[ 1 ], c2 ) ||
                                    SegmentIntersectsTriangle( position, photon.position, c2, plate2.positions[ 3 ], a2 );
                    }
                    if( intersect )
                        continue; // ray intersects with something

                    // distance:
                    const auto distance{ VecDist( photon.position, position ) };
                    const Real distanceFactor{ std::pow( 1 - ( distance < scene.wavelengthDecayDistance ? distance / scene.wavelengthDecayDistance : 1 ), 1 ) };
                    const auto wavelengthDecay{ VecAdd( scene.wavelengthDecay, VecMult( VecSub( { 1, 1, 1 }, scene.wavelengthDecay ), distanceFactor ) ) };
                    
                    // compute received power value:
                    const auto raySrcAngle{ -VecDot( photon.normal, rayNormal ) };
                    const auto received{ VecMult( VecMult( VecMult( photon.color, _plate.material.color ), wavelengthDecay ), raySrcAngle ) };
                    receiver = VecAdd( receiver, received );
                    emitter.color = VecAdd( emitter.color, VecMult( received, _plate.material.retransmission ) ); // cumulated energy transmission
                }
            }

            Progress();
        } );

        // convert emitters to photons:
        photons.clear();
        for( auto & plate : plates ) {
            for( auto & emitter : plate.emitters ) {
                const auto color{ VecMult( emitter.color, lightResolution ) };
                if( color[ 0 ] > epsilon && color[ 1 ] > epsilon && color[ 2 ] > epsilon ) {
                    // slightly move the photon above the plate
                    static constexpr Real shift{ 0.0000001 };
                    const auto position{ VecAdd( emitter.position, VecMult( plate.normal, shift ) ) };
                    photons.emplace_back( Photon{ position, plate.normal, color } );
                }
                emitter.color = Vector3D{ 0, 0, 0 }; // reset for next round
            }
        }
    }

    // computation duration:
    const auto duration{ std::chrono::high_resolution_clock::now() - t0 };
    const auto minutes { std::chrono::duration_cast< std::chrono::minutes >( duration ) };
    const auto seconds { std::chrono::duration_cast< std::chrono::seconds >( duration - minutes ) };
    const auto milliseconds { std::chrono::duration_cast< std::chrono::milliseconds >( duration - minutes - seconds ) };
    std::cout << "computation duration: " << minutes.count() << "m " << seconds.count() << "s " << milliseconds.count() << "ms" << std::endl;

    // convert received power values as RGB texture:
    std::for_each( std::execution::par_unseq, plates.begin(), plates.end(), [ & ]( auto & _plate ) {
        _plate.colors.resize( scene.textureWidth * scene.textureWidth );
        auto itColor{ _plate.colors.begin() };
        for( const auto receiver : _plate.receivers ) {
            const auto colorR{ static_cast< unsigned char >( std::clamp< Real >( receiver[ 0 ] / realMultiplier, 0, 1 ) * 255 ) };
            const auto colorG{ static_cast< unsigned char >( std::clamp< Real >( receiver[ 1 ] / realMultiplier, 0, 1 ) * 255 ) };
            const auto colorB{ static_cast< unsigned char >( std::clamp< Real >( receiver[ 2 ] / realMultiplier, 0, 1 ) * 255 ) };
            *itColor++ = { colorR, colorG, colorB };
        }
    } );

    // prepare 3D data for rendering:
    std::vector< float > vertices;
    std::vector< unsigned int > indices;
    const std::array< unsigned int, 6 > plateIndices{ 0, 1, 2, 2, 3, 0 };
    unsigned int offset{ 0 };
    for( const auto & plate : plates ) {
        for( int i{ 0 }; i < 4; ++i ) {
            for( const auto value : plate.positions[ i ] )
                vertices.emplace_back( static_cast< float >( value ) );
            for( const auto value : plate.textures[ i ] )
                vertices.emplace_back( static_cast< float >( value ) );
        }
        for( const auto i : plateIndices )
            indices.emplace_back( offset + i );
        offset += 4;
    }

    if( !::glfwInit() ) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    ::glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
    ::glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
    ::glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

    const int screenWidth{ 1600 };
    const int screenHeight{ 1200 };

    GLFWwindow * const pWindow{ ::glfwCreateWindow( screenWidth, screenHeight, "LightShot", nullptr, nullptr ) };
    if( pWindow == nullptr ) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        ::glfwTerminate();
        return -1;
    }

    ::glfwMakeContextCurrent( pWindow );
    ::glfwSetFramebufferSizeCallback( pWindow, []( GLFWwindow *, int _width, int _height ){
            ::glViewport( 0, 0, _width, _height );
        } );

    if( !::gladLoadGLLoader( reinterpret_cast< GLADloadproc >( ::glfwGetProcAddress ) ) ) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    auto vertexShader{ ::glCreateShader( GL_VERTEX_SHADER ) };
    const char * pVertexShaderSource{ R"_(
        #version 330 core
        layout ( location = 0 ) in vec3 aPos;
        layout ( location = 1 ) in vec2 aTexCoord;

        out vec2 TexCoord;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        void main()
        {
            gl_Position = projection * view * model * vec4( aPos, 1.0 );
            TexCoord = aTexCoord;
        }
    )_" };
    ::glShaderSource( vertexShader, 1, &pVertexShaderSource, nullptr );
    ::glCompileShader( vertexShader );

    auto fragmentShader{ ::glCreateShader( GL_FRAGMENT_SHADER ) };
    const char * pFragmentShaderSource{ R"_(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;

        uniform sampler2D faceTexture;

        void main() {
            FragColor = texture( faceTexture, TexCoord );
        }
    )_" };
    ::glShaderSource( fragmentShader, 1, &pFragmentShaderSource, nullptr );
    ::glCompileShader( fragmentShader );

    auto shaderProgram{ ::glCreateProgram() };
    ::glAttachShader( shaderProgram, vertexShader );
    ::glAttachShader( shaderProgram, fragmentShader );
    ::glLinkProgram( shaderProgram );
    ::glDeleteShader( vertexShader );
    ::glDeleteShader( fragmentShader );

    unsigned int vertexArray, verticesBufferObject, indicesBufferObject;
    ::glGenVertexArrays( 1, &vertexArray );
    ::glGenBuffers( 1, &verticesBufferObject );
    ::glGenBuffers( 1, &indicesBufferObject );
    
    ::glBindVertexArray( vertexArray );
    ::glBindBuffer( GL_ARRAY_BUFFER, verticesBufferObject );
    ::glBufferData( GL_ARRAY_BUFFER, vertices.size() * sizeof( float ), vertices.data(), GL_STATIC_DRAW );
    ::glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indicesBufferObject );
    ::glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof( unsigned int ), indices.data(), GL_STATIC_DRAW );

    ::glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof( float ), ( void * ) 0 );
    ::glEnableVertexAttribArray( 0 );
    ::glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof( float ), ( void * )( 3 * sizeof( float ) ) );
    ::glEnableVertexAttribArray( 1 );

    std::vector< unsigned int > textures;
    textures.resize( plates.size() );
    ::glGenTextures( static_cast< GLsizei >( textures.size() ), textures.data() );
    for( int i{ 0 }; i < plates.size(); ++i ) {
        ::glBindTexture( GL_TEXTURE_2D, textures[ i ] );
        ::glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, scene.textureWidth, scene.textureWidth, 0, GL_RGB, GL_UNSIGNED_BYTE, plates[ i ].colors.data() );
    }

    const auto cameraDepth{ static_cast< float >( std::max( scene.width, scene.height ) * scene.plateWidth ) };
    
    ::glEnable( GL_DEPTH_TEST );
    bool switchNearestTextureRendering{ false };
    while( !::glfwWindowShouldClose( pWindow ) ) {
        if( ::glfwGetKey( pWindow, GLFW_KEY_ESCAPE ) == GLFW_PRESS )
            ::glfwSetWindowShouldClose( pWindow, true );

        if( ::glfwGetKey( pWindow, GLFW_KEY_SPACE ) == GLFW_PRESS && !switchNearestTextureRendering )
            switchNearestTextureRendering = true;
        else
        if( ::glfwGetKey( pWindow, GLFW_KEY_SPACE ) == GLFW_RELEASE && switchNearestTextureRendering ) {
            switchNearestTextureRendering = false;
            nearestTextureRendering = !nearestTextureRendering;
        }
        
        ::glClearColor( 0, 0, 0, 1 );
        ::glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        ::glUseProgram( shaderProgram );
        
        float angle{ static_cast< float >( ::glfwGetTime() ) * 0.5f };

        ::glm::mat4 model{ ::glm::mat4{ 1.0f } };
        model = ::glm::rotate( model, ::glm::radians( 120.0f ), ::glm::vec3{ 1.0f, 0.0f, 0.0f } );
        model = ::glm::rotate( model, angle, ::glm::vec3{ 0.0f, 0.0f, 1.0f } );

        ::glm::mat4 view{ ::glm::translate( ::glm::mat4{ 1.0f }, ::glm::vec3{ 0.0f, 0.0f, -cameraDepth } ) };
        ::glm::mat4 projection{ ::glm::perspective( ::glm::radians( 45.0f ), static_cast< float >( screenWidth ) / screenHeight, 0.1f, 100.0f ) };

        ::glUniformMatrix4fv( ::glGetUniformLocation( shaderProgram, "model" ), 1, GL_FALSE, ::glm::value_ptr( model ) );
        ::glUniformMatrix4fv( ::glGetUniformLocation( shaderProgram, "view" ), 1, GL_FALSE, ::glm::value_ptr( view ) );
        ::glUniformMatrix4fv( ::glGetUniformLocation( shaderProgram, "projection" ), 1, GL_FALSE, ::glm::value_ptr( projection ) );

        for( int i{ 0 }; i < plates.size(); ++i ) {
            glBindTexture( GL_TEXTURE_2D, textures[ i ] );
            ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, nearestTextureRendering ? GL_NEAREST : GL_LINEAR );
            ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearestTextureRendering ? GL_NEAREST : GL_LINEAR );
            ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
            ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
            glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, ( void * )( i * 6 * sizeof( unsigned int ) ) );
        }

        ::glfwSwapBuffers( pWindow );
        ::glfwPollEvents();
    }

    ::glfwTerminate();
    return 0;
}
