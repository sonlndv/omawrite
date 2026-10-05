.pragma library

// Minimal force-directed layout for the graph view: build a node/edge model
// from Backend.linkGraph, then repeatedly apply repulsion + spring forces.
// No re-derivation of edges -- linkGraph is the single source of truth.

function buildModel(linkGraph) {
    var indexOf = {};
    var nodes = [];
    var edges = [];

    function nodeFor(id, label, broken) {
        if (indexOf[id] !== undefined)
            return indexOf[id];
        var idx = nodes.length;
        indexOf[id] = idx;
        nodes.push({id: id, label: label, broken: !!broken,
                    path: broken ? "" : id, x: 0, y: 0, vx: 0, vy: 0});
        return idx;
    }

    function labelFor(path) {
        var slash = path.lastIndexOf("/");
        var name = slash >= 0 ? path.slice(slash + 1) : path;
        return name.toLowerCase().endsWith(".md") ? name.slice(0, -3) : name;
    }

    for (var i = 0; i < linkGraph.length; i++) {
        var edge = linkGraph[i];
        var fromIdx = nodeFor(edge.from, labelFor(edge.from), false);
        var broken = !!edge.broken;
        var toId = broken ? ("broken:" + edge.rawTarget) : edge.to;
        var toIdx = nodeFor(toId, broken ? edge.rawTarget : labelFor(edge.to), broken);
        if (fromIdx !== toIdx)
            edges.push({source: fromIdx, target: toIdx, broken: broken});
    }

    return {nodes: nodes, edges: edges};
}

// Seeds any node still at (0,0) onto a circle so the simulation has
// somewhere to push from instead of every node starting stacked at once.
function seedPositions(nodes, width, height) {
    var n = nodes.length;
    for (var i = 0; i < n; i++) {
        var node = nodes[i];
        if (node.x !== 0 || node.y !== 0)
            continue;
        var angle = (i / Math.max(1, n)) * Math.PI * 2;
        var radius = Math.max(40, Math.min(width, height) * 0.35);
        node.x = width / 2 + Math.cos(angle) * radius;
        node.y = height / 2 + Math.sin(angle) * radius;
    }
}

// One simulation tick: all-pairs repulsion, spring attraction along edges,
// a weak pull to center, and velocity damping. Simple on purpose -- a
// fixed small iteration budget per frame keeps 500 nodes interactive
// without needing a spatial index.
function step(nodes, edges, width, height) {
    var n = nodes.length;
    var repulsion = 1800;
    var springLength = 70;
    var springStrength = 0.02;
    var damping = 0.82;
    var centerStrength = 0.006;

    for (var a = 0; a < n; a++) {
        for (var b = a + 1; b < n; b++) {
            var na = nodes[a], nb = nodes[b];
            var dx = na.x - nb.x, dy = na.y - nb.y;
            var distSq = dx * dx + dy * dy;
            if (distSq < 0.01) { dx = 0.1; dy = 0.1; distSq = 0.02; }
            var dist = Math.sqrt(distSq);
            var force = repulsion / distSq;
            var fx = (dx / dist) * force, fy = (dy / dist) * force;
            na.vx += fx; na.vy += fy;
            nb.vx -= fx; nb.vy -= fy;
        }
    }

    for (var e = 0; e < edges.length; e++) {
        var edge = edges[e];
        var s = nodes[edge.source], t = nodes[edge.target];
        var ex = t.x - s.x, ey = t.y - s.y;
        var edist = Math.sqrt(ex * ex + ey * ey) || 0.01;
        var sf = springStrength * (edist - springLength);
        var fx = (ex / edist) * sf, fy = (ey / edist) * sf;
        s.vx += fx; s.vy += fy;
        t.vx -= fx; t.vy -= fy;
    }

    var cx = width / 2, cy = height / 2;
    for (var i = 0; i < n; i++) {
        var node = nodes[i];
        node.vx += (cx - node.x) * centerStrength;
        node.vy += (cy - node.y) * centerStrength;
        node.vx *= damping;
        node.vy *= damping;
        node.x += node.vx;
        node.y += node.vy;
    }
}
