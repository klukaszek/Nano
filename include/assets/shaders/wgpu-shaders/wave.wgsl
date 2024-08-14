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
    let uv_aspect = vec2<f32>(uv.x * aspect, uv.y);

    // Parameters for the sine wave
    let frequency = 4.0;
    let amplitude = 0.4;
    let speed = 0.5;

    // Calculate the y-position of the sine wave
    let wave_y = sin((uv_aspect.x + uniforms.time * speed) * frequency) * amplitude + 0.5;

    // Calculate distance from current pixel to the sine wave
    let dist = abs(uv_aspect.y - wave_y);

    // Line thickness
    let thickness = 0.005;

    // Smoothstep for anti-aliasing
    let line = 1.0 - smoothstep(0.0, thickness, dist);

    // Wave color
    let waveColor = vec3<f32>(0.0, 0.7, 1.0);  // Light blue color

    // Background color
    let bgColor = vec3<f32>(0.1, 0.1, 0.1);

    let finalColor = mix(bgColor, waveColor, line);

    return vec4<f32>(finalColor, 1.0);
}
