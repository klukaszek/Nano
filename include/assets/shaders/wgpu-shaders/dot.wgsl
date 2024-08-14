struct Uniforms {
    @align(8) time: f32,
    @align(8) resolution: vec2<f32>,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    var uv = array<vec2<f32>, 3>(
        vec2<f32>(0.0, 1.0),
        vec2<f32>(2.0, 1.0),
        vec2<f32>(0.0, -1.0)
    );
    var output: VertexOutput;
    output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
    output.uv = uv[vertexIndex];
    return output;
}

@fragment
fn fs_main(@location(0) uv: vec2<f32>) -> @location(0) vec4<f32> {
    let aspect = uniforms.resolution.x / uniforms.resolution.y;
    let uv_aspect = vec2<f32>((uv.x - 0.5) * aspect, uv.y - 0.5);
    
    // Oscillating offset
    let offset = vec2<f32>(0.0, sin(uniforms.time * 2.0) * 0.2);
    
    // Calculate distance from current pixel to the oscillating center
    let dist = length(uv_aspect - offset);
    
    // Circle radius
    let radius = 0.2;
    
    // Smoothstep for anti-aliasing
    let circle = 1.0 - smoothstep(radius - 0.005, radius + 0.005, dist);
    
    // Circle color
    let circleColor = vec3<f32>(1.0, 0.5, 0.2);
    
    // Background color
    let bgColor = vec3<f32>(0.1, 0.1, 0.1);
    
    let finalColor = mix(bgColor, circleColor, circle);
    
    return vec4<f32>(finalColor, 1.0);
}
