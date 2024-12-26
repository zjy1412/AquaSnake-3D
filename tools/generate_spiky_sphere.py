import numpy as np
import math

def generate_sphere_vertices(radius, segments):
    vertices = []
    # 减少冗余顶点，只生成唯一的顶点
    for i in range(segments + 1):
        lat = math.pi * (-0.5 + float(i) / segments)
        for j in range(segments + 1):
            lon = 2 * math.pi * float(j) / segments
            x = math.cos(lat) * math.cos(lon) * radius
            y = math.sin(lat) * radius
            z = math.cos(lat) * math.sin(lon) * radius
            vertices.append((x, y, z))
    return vertices

def generate_sphere_faces(segments):
    faces = []
    for i in range(segments):
        for j in range(segments):
            first = i * (segments + 1) + j
            second = first + 1
            third = first + (segments + 1)
            fourth = third + 1
            # 只生成外表面的三角形
            faces.append((first + 1, second + 1, fourth + 1))
            faces.append((first + 1, fourth + 1, third + 1))
    return faces

def generate_spike(base_point, direction, height, base_radius, segments=8):
    vertices = []
    # 将基点和方向转换为numpy数组以便计算
    base_point = np.array(base_point)
    direction = np.array(direction)
    
    # 尖端点
    tip = base_point + direction * height
    vertices.append(tuple(tip))
    
    # 创建切平面的正交基
    # 确保选择一个不平行于direction的向量
    ref = np.array([1, 0, 0]) if abs(direction[1]) > 0.9 else np.array([0, 1, 0])
    # 第一个基向量
    u = np.cross(direction, ref)
    u = u / np.linalg.norm(u)
    # 第二个基向量
    v = np.cross(direction, u)
    v = v / np.linalg.norm(v)
    
    # 生成基座圆周点
    base_vertices = []
    for i in range(segments):
        angle = 2 * math.pi * i / segments
        # 在切平面上计算点的位置
        offset = base_radius * (u * math.cos(angle) + v * math.sin(angle))
        point = base_point + offset
        base_vertices.append(tuple(point))
    
    # 添加基座中心点
    # vertices.append(tuple(base_point))
    # 添加基座圆周点
    vertices.extend(base_vertices)
    
    return vertices

def fibonacci_sphere(samples):
    points = []
    phi = math.pi * (3. - math.sqrt(5.))  # 黄金角
    
    for i in range(samples):
        y = 1 - (i / float(samples - 1)) * 2
        radius = math.sqrt(1 - y * y)
        theta = phi * i
        x = math.cos(theta) * radius
        z = math.sin(theta) * radius
        points.append((x, y, z))
    
    return points

def normalize_vector(v):
    norm = math.sqrt(sum(x * x for x in v))
    return tuple(x / norm for x in v)

def save_obj(filename, vertices, faces):
    with open(filename, 'w') as f:
        f.write("# Spiky sphere OBJ file\n")
        # 写入顶点
        for v in vertices:
            f.write(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")
        # 写入面
        for face in faces:
            f.write(f"f {' '.join(str(i) for i in face)}\n")

def generate_spiky_sphere(radius=1.0, base_segments=20, spikes=50, spike_height=0.2, spike_radius=0.1):
    # 生成基础球体
    sphere_vertices = generate_sphere_vertices(radius, base_segments)
    sphere_faces = generate_sphere_faces(base_segments)
    
    # 生成尖刺分布点（使用fibonacci球算法）
    spike_points = fibonacci_sphere(spikes)
    
    # 为每个尖刺生成顶点和面
    all_vertices = sphere_vertices.copy()
    all_faces = sphere_faces.copy()
    
    for point in spike_points:
        # 将点归一化并缩放到球面
        normalized_point = normalize_vector(point)
        base_point = tuple(p * radius for p in normalized_point)
        
        # 生成尖刺顶点
        spike_verts = generate_spike(base_point, normalized_point, spike_height, spike_radius)
        # print(spike_verts)
        
        # 添加尖刺面
        base_index = len(all_vertices)
        all_vertices.extend(spike_verts)
        
        # 获取尖刺顶点数量（1个顶点 + n个基座点）
        segments = len(spike_verts) - 1
        
        # 添加尖刺侧面（三角形）
        for i in range(segments + 1):
            next_i = i % segments + 1
            # 侧面三角形（注意顶点顺序，确保法线朝外）
            # print(base_index + 1, base_index + 1 + i, base_index + 1 + next_i)
            all_faces.append((
                base_index + 1,           # 尖端点
                base_index + 1 + i,       # 当前基座点
                base_index + 1 + next_i   # 下一个基座点
            ))
    
    return all_vertices, all_faces

def main():
    # 生成带尖刺的球体
    vertices, faces = generate_spiky_sphere(
        radius=1.0,          # 球体半径
        base_segments=20,    # 基础球体分段数
        spikes=20,          # 尖刺数量
        spike_height=0.50,   # 尖刺高度
        spike_radius=0.25    # 尖刺基座半径
    )
    
    # 保存为OBJ文件
    save_obj("spiky_sphere.obj", vertices, faces)
    print("已生成spiky_sphere.obj文件")

if __name__ == "__main__":
    main() 