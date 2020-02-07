function copyToHeap(typedArray) {
  let size = typedArray.length * typedArray.BYTES_PER_ELEMENT;
  let offset = Module._malloc(size);
  Module.HEAPU8.set(new Uint8Array(typedArray.buffer), offset);
  return offset;
}

Module["dewarp"] = function(src, srcWidth, srcHeight, dstWidth, dstHeight, cpX, cpY, upperLeft, baseVec, normalVec, vertical, d) {
  let pSrc = copyToHeap(src);
  let pCpX = copyToHeap(cpX), pCpY = copyToHeap(cpY);
  let pUL = copyToHeap(upperLeft), pBV = copyToHeap(baseVec);
  let pNV = copyToHeap(normalVec), pVertical = copyToHeap(vertical);
  let pDst = Module._dewarp(pSrc, srcWidth, srcHeight, dstWidth, dstHeight, pCpX, pCpY, pUL, pBV, pNV, pVertical, d);
  let ret = new Uint8ClampedArray(4 * dstWidth * dstHeight);
  ret.set(new Uint8ClampedArray(Module.HEAPU8.buffer, pDst, 4 * dstWidth * dstHeight));
  Module._free(pSrc); Module._free(pDst);
  Module._free(pCpX); Module._free(pCpY);
  Module._free(pUL); Module._free(pBV);
  Module._free(pNV); Module._free(pVertical);
  return ret;
};