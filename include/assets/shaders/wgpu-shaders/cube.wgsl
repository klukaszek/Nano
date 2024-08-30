struct Uniforms {
    mvp: mat4x4<f32>,
    screen_width: f32,
    screen_height: f32,
    is_wireframe: u32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> position_buffer: array<f32>;
@group(0) @binding(2) var<storage, read> color_buffer: array<u32>;
@group(0) @binding(3) var<storage, read> index_buffer: array<u32>;

struct VertexInput {
    @builtin(vertex_index) vertexID: u32,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
    @location(1) barycentric: vec3<f32>,
};

@vertex
fn main_vertex(vertex: VertexInput) -> VertexOutput {
    var localToElement = array<u32, 6>(0u, 1u, 1u, 2u, 2u, 0u);

    var triangleIndex = vertex.vertexID / 6u;
    var localVertexIndex = vertex.vertexID % 6u;

    var elementIndexIndex = 3u * triangleIndex + localToElement[localVertexIndex];
    var elementIndex = index_buffer[elementIndexIndex];

    var position = vec4<f32>(
        position_buffer[3u * elementIndex + 0u],
        position_buffer[3u * elementIndex + 1u],
        position_buffer[3u * elementIndex + 2u],
        1.0
    );

    position = uniforms.mvp * position;

    var color_u32 = color_buffer[elementIndex];
    var color = vec4<f32>(
        f32((color_u32 >> 0u) & 0xFFu) / 255.0,
        f32((color_u32 >> 8u) & 0xFFu) / 255.0,
        f32((color_u32 >> 16u) & 0xFFu) / 255.0,
        f32((color_u32 >> 24u) & 0xFFu) / 255.0,
    );

    var barycentric = vec3<f32>(0.0, 0.0, 0.0);
    barycentric[localVertexIndex % 3u] = 1.0;

    var output: VertexOutput;
    output.position = position;
    output.color = color;
    output.barycentric = barycentric;

    return output;
}

@fragment
fn main_fragment(fragment: VertexOutput) -> @location(0) vec4<f32> {
    if uniforms.is_wireframe == 1u {
        // Wireframe mode
        let thickness: f32 = 1.0;
        let edge_factor = min(min(fragment.barycentric.x, fragment.barycentric.y), fragment.barycentric.z);
        let line_factor = smoothstep(0.0, thickness / uniforms.screen_width, edge_factor);

        if line_factor < 0.1 {
            return vec4<f32>(fragment.color.rgb, 1.0);  // White wireframe
        } else {
            discard;
        }
    }

    // Filled mode
    return fragment.color;
}
