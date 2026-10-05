import QtQuick
import QtQuick.Controls
import "GraphLayout.js" as GraphLayout

// Basic force-directed graph of the vault's wikilinks. Not an Obsidian
// clone: no pan/zoom minimap, no clustering, no node search -- a readable
// rendering of Backend.linkGraph that looks native to the Omarchy theme,
// with a click-to-open note.
Item {
    id: root

    // Incremented on every Canvas paint; read by the perf test to compute
    // real frames-per-second instead of trusting the simulation's own
    // cheap per-step cost, which says nothing about paint cost.
    property int paintCount: 0

    property var linkGraph: []
    property color backgroundColor: "#101010"
    property color nodeColor: "#5584aa"
    property color brokenColor: "#8a5050"
    property color edgeColor: "#555555"
    property color textColor: "#cccccc"
    property real textScale: 1

    signal nodeOpenRequested(string relativePath)

    property var model: ({nodes: [], edges: []})
    property bool settled: false

    function rebuild() {
        model = GraphLayout.buildModel(root.linkGraph);
        GraphLayout.seedPositions(model.nodes, width, height);
        settled = false;
        simTimer.start();
    }

    onLinkGraphChanged: if (visible) rebuild()
    onVisibleChanged: if (visible) rebuild()
    onWidthChanged: if (visible && model.nodes.length === 0) rebuild()

    Rectangle {
        anchors.fill: parent
        color: root.backgroundColor
    }

    // A fixed small step budget per frame, not a physics engine: enough
    // ticks to settle a few hundred nodes within a couple of seconds, cheap
    // enough that the UI thread stays responsive. Stops once the layout
    // has visibly settled so idle CPU drops back to zero.
    Timer {
        id: simTimer
        interval: 16
        repeat: true
        running: false
        property int ticksRemaining: 0
        onTriggered: {
            GraphLayout.step(root.model.nodes, root.model.edges, root.width, root.height);
            canvas.requestPaint();
            ticksRemaining--;
            if (ticksRemaining <= 0) {
                stop();
                root.settled = true;
            }
        }
        onRunningChanged: if (running) ticksRemaining = 180
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative

        onPaint: {
            root.paintCount++;
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            var nodes = root.model.nodes;
            var edges = root.model.edges;

            // One path per edge style instead of one per edge: far fewer
            // draw calls is what keeps 500+ nodes paintable every frame
            // while the simulation is still animating.
            ctx.lineWidth = 1;
            ctx.strokeStyle = root.edgeColor;
            ctx.setLineDash([]);
            ctx.beginPath();
            for (var e = 0; e < edges.length; e++) {
                var edge = edges[e];
                if (edge.broken)
                    continue;
                var s = nodes[edge.source], t = nodes[edge.target];
                ctx.moveTo(s.x, s.y);
                ctx.lineTo(t.x, t.y);
            }
            ctx.stroke();

            ctx.strokeStyle = root.brokenColor;
            ctx.setLineDash([3, 3]);
            ctx.beginPath();
            for (var eb = 0; eb < edges.length; eb++) {
                var brokenEdge = edges[eb];
                if (!brokenEdge.broken)
                    continue;
                var bs = nodes[brokenEdge.source], bt = nodes[brokenEdge.target];
                ctx.moveTo(bs.x, bs.y);
                ctx.lineTo(bt.x, bt.y);
            }
            ctx.stroke();
            ctx.setLineDash([]);

            var radius = Math.max(3, Math.round(4 * root.textScale));
            // Labels are the expensive part of this paint (per-glyph
            // shaping x hundreds of nodes), so only draw them once the
            // layout has settled; the moving swarm reads fine as plain
            // dots while it's still finding its shape.
            var drawLabels = root.settled;
            if (drawLabels) {
                var fontSize = Math.max(9, Math.round(10 * root.textScale));
                ctx.font = fontSize + "px sans-serif";
                ctx.textBaseline = "middle";
                ctx.fillStyle = root.textColor;
            }
            for (var i = 0; i < nodes.length; i++) {
                var node = nodes[i];
                ctx.beginPath();
                ctx.arc(node.x, node.y, radius, 0, Math.PI * 2);
                ctx.fillStyle = node.broken ? root.brokenColor : root.nodeColor;
                ctx.fill();

                if (drawLabels) {
                    ctx.fillStyle = root.textColor;
                    ctx.fillText(node.label, node.x + radius + 4, node.y);
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: function(mouse) {
            var nodes = root.model.nodes;
            var hitRadius = Math.max(8, Math.round(8 * root.textScale));
            for (var i = 0; i < nodes.length; i++) {
                var node = nodes[i];
                var dx = mouse.x - node.x, dy = mouse.y - node.y;
                if (dx * dx + dy * dy <= hitRadius * hitRadius) {
                    if (!node.broken && node.path.length > 0)
                        root.nodeOpenRequested(node.path);
                    return;
                }
            }
        }
    }

    Label {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 10
        text: root.model.nodes.length + " notes, " + root.model.edges.length + " links"
        color: root.textColor
        opacity: 0.6
        font.pixelSize: Math.max(9, Math.round(10 * root.textScale))
    }
}
