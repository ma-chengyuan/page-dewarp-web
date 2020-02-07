<template>
  <v-app v-resize="updateCanvasSize">
    <v-app-bar app color="primary" dark>
      <h1>Page Dewarp</h1>
    </v-app-bar>
    <v-content style="margin: 2% 2%">
      <div>
        <v-container fluid>
          <v-row>
            <v-col :cols="mobile ? 12 : 6">
              <v-card outlined class="pa-1">
                <div id="canvasDiv" style="position: relative;">
                  <canvas id="canvas" style="position: absolute; top: 0; left: 0;"/>
                  <canvas id="magnifier" style="position: absolute; top: 0; left: 0; width: 50%; height: 50%;"/>
                </div>
              </v-card>
            </v-col>
            <v-col :cols="mobile ? 12 : 6">
              <v-card outlined class="mx-auto pa-3">
                <div style="display: flex; flex-direction: row" class="my-1">
                  <v-file-input label="Select image file" @change="updateImage"/>
                  <div class="mx-3"/>
                  <v-btn-toggle class="align-center" v-model="splineNodePosition" mandatory dense>
                    <v-btn value="lower">
                      <v-icon>mdi-format-vertical-align-bottom</v-icon>
                    </v-btn>
                    <v-btn value="upper">
                      <v-icon>mdi-format-vertical-align-top</v-icon>
                    </v-btn>
                  </v-btn-toggle>
                </div>
                <v-slider class="align-center" v-model="viewDepth" min="0.2" max="2.0" step="0.001"
                          label="View depth : Width">
                  <template v-slot:append>
                    <v-text-field v-model="viewDepth" class="mt-0 pt-0" single-line type="number" style="width: 60px;"/>
                  </template>
                </v-slider>
                <v-slider class="align-center" v-model="leftSlope" min="-4" max="4" step="0.001"
                          label="Left Slope">
                  <template v-slot:append>
                    <v-text-field v-model="leftSlope" class="mt-0 pt-0" single-line type="number" style="width: 60px;"/>
                  </template>
                </v-slider>
                <v-slider class="align-center" v-model="rightSlope" min="-4" max="4" step="0.001"
                          label="Right Slope">
                  <template v-slot:append>
                    <v-text-field v-model="rightSlope" class="mt-0 pt-0" single-line type="number"
                                  style="width: 60px;"/>
                  </template>
                </v-slider>
                <v-slider class="align-center" v-model="resultWidth" min="100" max="3000" step="100"
                          label="Output width in pixels">
                  <template v-slot:append>
                    <v-text-field v-model="resultWidth" class="mt-0 pt-0" single-line type="number"
                                  style="width: 60px;"/>
                  </template>
                </v-slider>
                <div style="display: flex; flex-direction: row" class="my-1">
                  <v-btn class="align-center ma-auto" :disabled="working" @click="dewarp">Dewarp</v-btn>
                </div>
              </v-card>
              <div style="min-height: 20px"/>
              <v-card id="resultDiv" class="mx-auto">
                <v-progress-linear :key="progress" class="align-center" :value="progress"/>
                <canvas class="pt-2" id="result"/>
              </v-card>
            </v-col>
          </v-row>
        </v-container>
      </div>
      <img id="imageOriginal" :src="imageUrl" alt="" style="display: none;"/>
      <canvas id="original" style="display: none;"/>
    </v-content>
  </v-app>
</template>

<script>
  /* eslint-disable no-console */

  import {fabric} from "fabric"
  // eslint-disable-next-line no-unused-vars
  import {
    det,
    subtract,
    add as vadd, // It seems that there is another Vue function named "add". "v" for vector.
    concat,
    ctranspose,
    dot,
    cross,
    multiply,
    divide,
    norm,
    zeros,
    pow,
    lusolve,
    // format,
  } from "mathjs"

  // eslint-disable-next-line no-unused-vars
  import wasmModule from "./wasm/dewarp.js"
  import wasmSource from "./wasm/dewarp.wasm"

  export default {
    name: 'App',
    created() {
      // Number of spline nodes (knots)
      this.nodeCount = 5;
      // Number of extra nodes in a range between two knots, used to plot the interpolation curve
      this.nodeCountBetweenKnots = 8;
      // 2 end point node (lower left + lower right) + nodes + (nodes + 1) intervals * nods per range
      this.nodeCountTotal = this.nodeCount + 2 + this.nodeCountBetweenKnots * (this.nodeCount + 1);

      // Magnification of magnifier
      this.magnification = 1.5;

      // Styling...
      this.cornerNodeRadius = 12;
      this.curveNodeRadius = 8;
      this.boundingBoxOptions = {
        stroke: "orangered", strokeWidth: 1,
        strokeLineJoin: "bevil",
        fill: null, left: 0, top: 0,
        opacity: 0.8, selectable: false,
        objectCaching: false
      };
      this.hoveringBoxOptions = {
        stroke: "green", strokeWidth: 1,
        strokeLineJoin: "bevil",
        fill: null, left: 0, top: 0,
        opacity: 0.8, selectable: false,
        objectCaching: false
      };
      this.curveOptions = {
        stroke: "blue", fill: null,
        strokeWidth: 1,
        selectable: false,
        objectCaching: false
      };
      this.curveNodeOptions = {
        opacity: 0.8, radius: this.curveNodeRadius, stroke: "red", fill: null
      };
      this.cornerNodeOptions = {
        opacity: 0.8, radius: this.cornerNodeRadius, stroke: "red", fill: null
      };

      this.wasmModule = wasmModule({
        locateFile(path) {
          return path.endsWith(".wasm") ? wasmSource : path;
        }
      });
      this.wasmModule.onRuntimeInitialized = () => {
        console.log("Completed WebAssembly initialization!");
      };
      this.webAssemblyMode = true;
    },
    mounted() {
      this.updateCanvasSize();
      // Initialize canvas
      this.canvas = new fabric.Canvas("canvas");
      this.vanillaCanvas = document.getElementById("canvas");
      this.magnifier = document.getElementById("magnifier");
      this.canvasDiv = document.getElementById("canvasDiv");
      this.resultDiv = document.getElementById("resultDiv");
      this.originalCanvas = document.getElementById("original");
      this.resultCanvas = document.getElementById("result");
      this.imageOriginal = document.getElementById("imageOriginal");
      this.canvas.selection = false;
      this.resultDiv.style.display = "none";

      this.boundingBox = new fabric.Polygon(this.zeroInitFabricCoords(this.corners.length), this.boundingBoxOptions);
      this.canvas.add(this.boundingBox);
      this.canvasResizeListeners.push(this.updateBoundingBox);

      this.hoveringBox = new fabric.Polygon(this.zeroInitFabricCoords(this.corners.length), this.hoveringBoxOptions);
      this.canvas.add(this.hoveringBox);
      this.canvasResizeListeners.push(this.updateHoveringBox);

      this.lowerCurve = new fabric.Polyline(this.zeroInitFabricCoords(this.nodeCountTotal), this.curveOptions);
      this.upperCurve = new fabric.Polyline(this.zeroInitFabricCoords(this.nodeCountTotal), this.curveOptions);
      this.canvas.add(this.lowerCurve);
      this.canvas.add(this.upperCurve);
      this.canvasResizeListeners.push(this.updateBothCurves);

      // Add objects to move spline nodes (knots) around
      for (let i = 0; i < this.nodeCount; i++) {
        // Spread nodes evenly
        this.splineNodes.push([(i + 1) / (this.nodeCount + 1), 0]);
        let node = new fabric.Circle(this.curveNodeOptions);
        node.lockRotation = node.lockUniScaling = true;
        node.hasControls = node.hasBorders = false;
        node.on("moving", this.updateCurveNodeOnManipulation(i));
        this.canvasResizeListeners.push(this.updateCurveNodeDisplay(i, node));
        this.curveUpdateListeners.push(this.updateCurveNodeDisplay(i, node));
        this.canvas.add(node);
      }

      // Finally, add control object for 4 critical vertices
      this.corners.forEach((name) => {
        let node = new fabric.Circle(this.cornerNodeOptions);
        node.lockRotation = node.lockUniScaling = true;
        node.hasControls = node.hasBorders = false;
        node.on("moving", this.updateCornerNodeOnManipulation(name));
        this.canvasResizeListeners.push(this.updateCornerNodeDisplay(name, node));
        this.canvas.add(node);
      });

      this.hideMagnifier();
      this.canvas.on("object:moving", this.showAndUpdateMagnifier);
      this.canvas.on("object:moved", this.hideMagnifier);

      this.updateEverything();
    },
    methods: {
      // Convert internal coordinate to canvas coordinate
      toCanvasCoord(v) {
        return [(v[0] + 0.5) * this.canvasWidth, v[1] * this.canvasWidth + 0.5 * this.canvasHeight];
      },
      // Convert canvas coordinate to internal coordinate
      fromCanvasCoord(v) {
        return [v[0] / this.canvasWidth - 0.5, (v[1] - 0.5 * this.canvasHeight) / this.canvasWidth];
      },
      zeroInitFabricCoords(count) {
        let ret = [];
        for (let i = 0; i < count; i++)
          ret.push({x: 0, y: 0});
        return ret;
      },
      checkInBound(coord) {
        let onCanvas = this.toCanvasCoord(coord);
        let cw = this.canvasWidth * 0.05;
        let ch = this.canvasHeight * 0.05;
        this.$set(coord, 0, onCanvas[0] < cw ? this.fromCanvasCoord([cw, 0])[0] :
          (onCanvas[0] > this.canvasWidth - cw ? this.fromCanvasCoord([this.canvasWidth - cw, 0])[0] : coord[0]));
        this.$set(coord, 1, onCanvas[1] < ch ? this.fromCanvasCoord([0, ch])[1] :
          (onCanvas[1] > this.canvasHeight - ch ? this.fromCanvasCoord([0, this.canvasHeight - ch])[1] : coord[1]));
      },
      // Update all canvas elements and make sure all objects are where they should be (mathematically)
      updateCanvasSize() {
        this.mobile = window.innerHeight / window.innerWidth > 1;
        this.$nextTick(() => {
          fabric.Image.fromURL(this.imageUrl, image => {
            this.canvasWidth = this.canvasDiv.clientWidth;
            this.canvasHeight = image.width !== 0 ? image.height / image.width * this.canvasDiv.clientWidth : 0;
            /*
            if (this.canvasHeight > window.innerHeight - 128) {
              this.canvasHeight = window.innerHeight - 128;
              this.canvasWidth = image.height !== 0 ? image.width / image.height * this.canvasHeight : 0;
            }
            */
            this.canvas.setWidth(this.canvasWidth).setHeight(this.canvasHeight);
            for (let i = 0; i < this.corners.length; i++)
              this.checkInBound(this[this.corners[i]]);
            this.canvasResizeListeners.forEach(listener => listener());
            this.canvas.setBackgroundImage(image, this.canvas.renderAll.bind(this.canvas), {
              scaleX: this.canvas.width / image.width,
              scaleY: this.canvas.height / image.height
            });
          });
        });
      },
      updateImage(file) {
        if (file) {
          this.imageUrl = window.URL.createObjectURL(file);
          this.imageOriginal.crossOrigin = "Anonymous";
          this.updateCanvasSize();
        }
      },
      updateBoundingBox() {
        for (let i = 0; i < this.corners.length; i++) {
          let coord = this.toCanvasCoord(this[this.corners[i]]);
          // Subtract strokeWidth / 3 for magic
          this.boundingBox.points[i].x = coord[0] - this.boundingBox.strokeWidth / 3;
          this.boundingBox.points[i].y = coord[1] - this.boundingBox.strokeWidth / 3;
        }
        this.boundingBox.setCoords();
      },
      updateHoveringBox() {
        for (let i = 0; i < this.corners.length; i++) {
          let coord = this.toCanvasCoord(this.project(vadd(this[this.corners[i] + "3D"], multiply(this.normalVec, 0.05))));
          this.hoveringBox.points[i].x = coord[0] - this.hoveringBox.strokeWidth / 3;
          this.hoveringBox.points[i].y = coord[1] - this.hoveringBox.strokeWidth / 3;
        }
        this.hoveringBox.setCoords();
      },
      updateCurve(position) {
        let curve = this[position + "Curve"];
        let startingFrom = this[position + "Left3D"];
        let spline = this.clampedSpline();
        for (let i = 0; i <= this.nodeCount; i++) {
          let [a, b, c, d] = spline[i];
          let xLeft = i === 0 ? 0 : this.splineNodes[i - 1][0];
          let xRight = i === this.nodeCount ? 1 : this.splineNodes[i][0];
          let step = (xRight - xLeft) / (this.nodeCountBetweenKnots + 1);
          for (let j = 0; j <= this.nodeCountBetweenKnots; j++) {
            let x = xLeft + j * step;
            let y = ((d * x + c) * x + b) * x + a;
            let coord = this.toCanvasCoord(this.project(vadd(startingFrom, vadd(
              multiply(this.baseVec, x), multiply(this.normalVec, y)
            ))));
            curve.points[i * (this.nodeCountBetweenKnots + 1) + j].x = coord[0];
            curve.points[i * (this.nodeCountBetweenKnots + 1) + j].y = coord[1];
          }
        }
        let coord = this.toCanvasCoord(this[position + "Right"]);
        curve.points[this.nodeCountTotal - 1].x = coord[0];
        curve.points[this.nodeCountTotal - 1].y = coord[1];
        curve.setCoords();
      },
      updateBothCurves() {
        this.updateCurve("lower");
        this.updateCurve("upper");
      },
      updateCurveNodeOnManipulation(ith) {
        return (event) => {
          let coord = subtract(this.projectBackToBasePlane(this.fromCanvasCoord([
            event.target.left + this.curveNodeRadius, event.target.top + this.curveNodeRadius
          ])), this[this.splineNodePosition + "Left3D"]);
          this.splineNodes[ith][0] = dot(coord, this.baseVec) / dot(this.baseVec, this.baseVec);
          this.splineNodes[ith][1] = dot(coord, this.normalVec) / dot(this.normalVec, this.normalVec);
          // Don't forget to update the curve
          this.updateBothCurves();
        };
      },
      updateCurveNodeDisplay(ith, node) {
        return () => {
          let coord = this.toCanvasCoord(this.project(vadd(this[this.splineNodePosition + "Left3D"],
            vadd(multiply(this.baseVec, this.splineNodes[ith][0]),
              multiply(this.normalVec, this.splineNodes[ith][1])))));
          node.set("left", coord[0] - this.curveNodeRadius);
          node.set("top", coord[1] - this.curveNodeRadius);
          node.setCoords();
        };
      },
      updateCornerNodeOnManipulation(name) {
        return (event) => {
          this[name] = this.fromCanvasCoord(
            [event.target.left + this.cornerNodeRadius, event.target.top + this.cornerNodeRadius]);
          this.updateEverything();
        };
      },
      updateCornerNodeDisplay(name, node) {
        return () => {
          let newCoord = this.toCanvasCoord(this[name]);
          node.set("left", newCoord[0] - this.cornerNodeRadius);
          node.set("top", newCoord[1] - this.cornerNodeRadius);
          node.setCoords();
        }
      },
      updateEverything() {
        this.updateBoundingBox();
        this.updateHoveringBox();
        this.updateBothCurves();
        this.curveUpdateListeners.forEach((listener) => listener());
      },
      showAndUpdateMagnifier(event) {
        this.magnifier.style.zIndex = "1";
        let left = event.pointer.x < this.canvasWidth * 0.5;
        let upper = event.pointer.y < this.canvasHeight * 0.5;
        this.magnifier.style.left = left ? "50%" : "0";
        this.magnifier.style.top = upper ? "0" : "50%";
        let k = this.vanillaCanvas.width / this.canvasWidth;
        let cropWidth = this.canvasWidth * k * 0.5 / this.magnification;
        let cropHeight = this.canvasHeight * k * 0.5 / this.magnification;
        this.magnifier.width = cropWidth;
        this.magnifier.height = cropHeight;
        let sx = Math.max(event.pointer.x * k - cropWidth / 2, 0);
        sx = Math.min(sx, this.canvasWidth * k - cropWidth);
        let sy = Math.max(event.pointer.y * k - cropHeight / 2, 0);
        sy = Math.min(sy, this.canvasHeight * k - cropHeight);
        let ctx = this.magnifier.getContext("2d");
        ctx.drawImage(this.vanillaCanvas, sx, sy, cropWidth, cropHeight, 0, 0, cropWidth, cropHeight);
      },
      hideMagnifier() {
        this.magnifier.style.zIndex = "-1";
      },
      // Calculate the point on basePlane based on its projection, v.
      projectBackToBasePlane(v) {
        let plane = this.basePlane;
        let lhs = (plane[0] * v[0] + plane[1] * v[1]) / this.viewDepth + plane[2];
        let rhs = plane[3] - plane[0] * v[0] - plane[1] * v[1];
        let z = rhs / lhs;
        return concat(multiply(v, 1 + z / this.viewDepth), [z]);
      },
      project(v) {
        return [v[0] / (1 + v[2] / this.viewDepth), v[1] / (1 + v[2] / this.viewDepth)];
      },
      clampedSpline() {
        let nodes = this.splineNodes.map(fabric.util.object.clone);
        nodes.unshift([0, 0]);
        nodes.push([1, 0]);
        let n = nodes.length - 1; // nodes.length knots, so nodes.length ranges and piecewise curves
        let A = zeros(4 * n, 4 * n); // 4 parameters for each curve
        let b = zeros(4 * n);
        for (let i = 0; i < n; i++) {
          // Equation 4i and 4i + 1 ensures continuity of the spline
          b.set([4 * i], nodes[i][1]);
          for (let j = 0; j < 4; j++)
            A.set([4 * i, 4 * i + j], pow(nodes[i][0], j));
          b.set([4 * i + 1], nodes[i + 1][1]);
          for (let j = 0; j < 4; j++)
            A.set([4 * i + 1, 4 * i + j], pow(nodes[i + 1][0], j));
          if (i < n - 1) {
            // Equation 4i + 2 and 4i + 3 ensures continuity of both the 1st and 2nd derivative
            A.set([4 * i + 2, 4 * i + 1], 1);
            A.set([4 * i + 2, 4 * i + 2], 2 * nodes[i + 1][0]);
            A.set([4 * i + 2, 4 * i + 3], 3 * pow(nodes[i + 1][0], 2));
            A.set([4 * i + 2, 4 * i + 5], -1);
            A.set([4 * i + 2, 4 * i + 6], -2 * nodes[i + 1][0]);
            A.set([4 * i + 2, 4 * i + 7], -3 * pow(nodes[i + 1][0], 2));
            A.set([4 * i + 3, 4 * i + 2], 2);
            A.set([4 * i + 3, 4 * i + 3], 6 * nodes[i + 1][0]);
            A.set([4 * i + 3, 4 * i + 6], -2);
            A.set([4 * i + 3, 4 * i + 7], -6 * nodes[i + 1][0]);
          }
        }
        // Equation 4n - 2 and 4n - 1 set constraint on slope on both ends
        A.set([4 * n - 2, 1], 1);
        A.set([4 * n - 2, 2], 2 * nodes[0][0]);
        A.set([4 * n - 2, 3], 3 * pow(nodes[0][0], 2));
        b.set([4 * n - 2], this.leftSlope);
        A.set([4 * n - 1, 4 * n - 3], 1);
        A.set([4 * n - 1, 4 * n - 2], 2 * nodes[n][0]);
        A.set([4 * n - 1, 4 * n - 1], 3 * pow(nodes[n][0], 2));
        b.set([4 * n - 1], this.rightSlope);
        let sln = lusolve(A, b);
        // Convert mathjs row vector to desired format
        let ret = [];
        for (let i = 0; i < n; i++)
          ret.push([sln.get([4 * i, 0]), sln.get([4 * i + 1, 0]), sln.get([4 * i + 2, 0]), sln.get([4 * i + 3, 0])]);
        return ret;
      },
      criticalPointsSimpson(spline, res) {
        const step = 0.0001;
        let len = 0, t = [];
        for (let i = 0; i <= this.nodeCount; i++) {
          let [a, b, c, d] = spline[i];
          let xLeft = i === 0 ? 0 : this.splineNodes[i - 1][0];
          let xRight = i === this.nodeCount ? 1 : this.splineNodes[i][0];
          let f = x => Math.hypot(1, ((3 * d * x + 2 * c) * x + b));
          for (let x = xLeft; x < xRight; x += step) {
            let h = Math.min(xRight - x, step);
            len += (f(x) + 4 * f(x + h / 2) + f(x + h)) * h / 6;
            t.push([x, len, ((d * x + c) * x + b) * x + a]);
          }
        }
        let ret = [], p = 0;
        for (let i = 1; i <= res; i++) {
          let l = i / res * len;
          while (p + 1 < t.length && t[p + 1][1] <= l) p++;
          let lt = t[p], rt = p + 1 === t.length ? [1, len, 0] : t[p + 1];
          ret.push(l - lt[1] <= rt[1] - l ? [lt[0], lt[2]] : [rt[0], rt[2]]);
        }
        return [len, ret];
      },
      dewarp() {
        this.working = true;
        setTimeout(() => {
          this.resultDiv.style.display = "initial";
          let srcWidth = this.imageOriginal.width;
          let srcHeight = this.imageOriginal.height;
          this.originalCanvas.width = srcWidth;
          this.originalCanvas.height = srcHeight;
          let contextOriginal = this.originalCanvas.getContext("2d");
          contextOriginal.drawImage(this.imageOriginal, 0, 0);
          let [len, criticalPoints] = this.criticalPointsSimpson(this.clampedSpline(), this.resultWidth);
          let vertical = subtract(this.lowerLeft3D, this.upperLeft3D);
          let dstWidth = criticalPoints.length;
          let dstHeight = Math.round(dstWidth / (len * norm(this.baseVec)) * norm(vertical));
          this.resultCanvas.width = dstWidth;
          this.resultCanvas.height = dstHeight;
          this.resultCanvas.style.width = "100%";
          let contextResult = this.resultCanvas.getContext("2d");
          this.resultCanvas.style.display = "initial";
          let imageDataOriginal = contextOriginal.getImageData(0, 0, srcWidth, srcHeight);

          this.progress = 0;

          if (this.webAssemblyMode) {
            let cpX = [], cpY = [];
            criticalPoints.forEach(p => {
              cpX.push(p[0]);
              cpY.push(p[1]);
            });
            let ret = this.wasmModule.dewarp(
              imageDataOriginal.data, srcWidth, srcHeight, dstWidth, dstHeight,
              new Float32Array(cpX), new Float32Array(cpY),
              new Float32Array(this.upperLeft3D), new Float32Array(this.baseVec),
              new Float32Array(this.normalVec), new Float32Array(vertical),
              this.viewDepth);
            contextResult.putImageData(new ImageData(ret, dstWidth, dstHeight), 0, 0);
            this.working = false;
            let link = document.createElement("a");
            link.download = "Result.jpg";
            link.href = this.resultCanvas.toDataURL("image/jpeg", 0.8);
            link.click();
          } else {
            let progressDelta = 100 / dstWidth;
            let completed = 0;
            let imageDataResult = contextResult.createImageData(dstWidth, dstHeight);
            for (let x = 0; x < dstWidth; x++) {
              const top = vadd(this.upperLeft3D,
                multiply(this.baseVec, criticalPoints[x][0]), multiply(this.normalVec, criticalPoints[x][1]));
              setTimeout(() => {
                for (let y = 0; y < dstHeight; y++) {
                  let coord = multiply(this.toCanvasCoord(this.project(
                    vadd(top, multiply(vertical, y / dstHeight)))), srcWidth / this.canvasWidth);
                  let di = (y * dstWidth + x) * 4;
                  let si = (Math.round(coord[1]) * srcWidth + Math.round(coord[0])) * 4;
                  imageDataResult.data[di] = imageDataOriginal.data[si];
                  imageDataResult.data[di + 1] = imageDataOriginal.data[si + 1];
                  imageDataResult.data[di + 2] = imageDataOriginal.data[si + 2];
                  imageDataResult.data[di + 3] = imageDataOriginal.data[si + 3];
                }
                this.progress += progressDelta;
                if (++completed === dstWidth) {
                  contextResult.putImageData(imageDataResult, 0, 0);
                  this.working = false;
                  let link = document.createElement("a");
                  link.download = "Result.png";
                  link.href = this.resultCanvas.toDataURL();
                  link.click();
                }
              }, 0);
            }
          }
        }, 0);
      }
    },
    computed: {
      // The upperLeftDepth and upperRightDepth are solved using Cramer's Rule
      // Determinant of the equation for upperLeftDepth and upperRightDepth
      // Derivation: https://alan20210202.github.io/2019/12/24/Dewarp1/
      det() {
        return det(ctranspose(concat(
          [subtract(this.upperLeft, this.lowerRight)],
          [subtract(this.lowerRight, this.upperRight)], 0)));
      },
      upperLeftDepth() {
        return det(ctranspose(concat(
          [subtract(this.lowerLeft, this.lowerRight)],
          [subtract(this.lowerRight, this.upperRight)], 0))) / this.det;
      },
      upperRightDepth() {
        return det(ctranspose(concat(
          [subtract(this.upperLeft, this.lowerRight)],
          [subtract(this.lowerLeft, this.lowerRight)], 0))) / this.det;
      },
      lowerRightDepth() {
        return this.upperRightDepth - this.upperLeftDepth + 1
      },
      // 3D coordinates of 4 critical vertices
      lowerLeft3D() {
        return concat(this.lowerLeft, [0]);
      },
      upperLeft3D() {
        return concat(multiply(this.upperLeft, this.upperLeftDepth), [(this.upperLeftDepth - 1) * this.viewDepth]);
      },
      upperRight3D() {
        return concat(multiply(this.upperRight, this.upperRightDepth), [(this.upperRightDepth - 1) * this.viewDepth]);
      },
      lowerRight3D() {
        return concat(multiply(this.lowerRight, this.lowerRightDepth), [(this.lowerRightDepth - 1) * this.viewDepth]);
      },
      // The plane perpendicular to the page plane and on which the bottom edge of the page lies
      basePlane() {
        let diff = cross(this.normalVec, subtract(this.lowerLeft3D, this.lowerRight3D));
        return concat(diff, [dot(diff, this[this.splineNodePosition + "Left3D"])]);
      },
      // The horizontal edge vector of the page plane, from left to right
      baseVec() {
        return subtract(this.lowerRight3D, this.lowerLeft3D);
      },
      // The normal vector of basePlane
      normalVec() {
        let vec = cross(subtract(this.lowerLeft3D, this.lowerRight3D), subtract(this.lowerLeft3D, this.upperLeft3D));
        return divide(vec, norm(vec));
      },
    },
    watch: {
      viewDepth() {
        this.updateEverything();
        this.canvas.renderAll();
      },
      leftSlope() {
        this.updateBothCurves();
        this.canvas.renderAll();
      },
      rightSlope() {
        this.updateBothCurves();
        this.canvas.renderAll();
      },
      splineNodePosition() {
        this.curveUpdateListeners.forEach((listener) => listener());
        this.canvas.renderAll();
      }
    },
    data() {
      return {
        imageUrl: require("./assets/usage.jpg"),
        // Below are DOM elements
        canvasDiv: null,
        resultDiv: null,
        imageOriginal: null,
        canvas: null,
        vanillaCanvas: null,
        originalCanvas: null,
        resultCanvas: null,
        magnifier: null,
        // Parameters
        canvasWidth: 0,
        canvasHeight: 0,
        viewDepth: 25.5 / 25.1, // Empirical default value, based on my phone
        leftSlope: 0,
        rightSlope: 0,
        lowerLeft: [-0.4, 0.4],
        upperLeft: [-0.4, -0.4],
        lowerRight: [0.4, 0.4],
        upperRight: [0.4, -0.4],
        corners: ["lowerLeft", "upperLeft", "upperRight", "lowerRight"],
        boundingBox: null,
        hoveringBox: null,
        lowerCurve: null,
        upperCurve: null,
        mobile: false,
        splineNodes: [], // Spline nodes except the two critical nodes on both ends
        splineNodePosition: "lower",
        canvasResizeListeners: [], // Will be called when canvas resize / image is changed
        curveUpdateListeners: [], // Will be called when curve changes
        resultWidth: 1600,
        progress: 0,
        working: false,
      }
    }
  };
</script>
