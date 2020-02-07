# page-dewarp-web

Dewarps book page in images.
Demo website: https://alan20210202.github.io/dewarp/

It is a personal hobby project, more information can be found in
my blog posts:

* https://alan20210202.github.io/2019/12/24/Dewarp1/
* https://alan20210202.github.io/2019/12/28/Dewarp2/
* https://alan20210202.github.io/2020/01/10/Dewarp3/
* https://alan20210202.github.io/2020/01/14/WebAssembly/

## Features

* Safe: This is a pure web application, running directly in the browser with no backend
and it never uploads the original image to any servers.
* Fast: Optimized with WebAssembly technology, the app runs fast even on mobile platforms. 

## Limitations

* An assumption is made in the algorithm that the two vertical edges of the page in an image must be **parallel**. 
This should cover most use cases. 
* It still requires users to manually outline page boundaries, which impairs user exprience and can be possibly 
automated with CV. 
* It doesn't automatically rotate the image to the correct orientation. One may find an image in different orientation 
in the app from that in one's gallery app. 
* On the technical side, I am new to JavaScript and Web development and I wrote all code in a single Vue Component file.
Though I try to keep the code tidy there might be readability issues, and some code I wrote may not conform to best practices.

## Sample results

![lhyM24.jpg](https://s2.ax1x.com/2020/01/10/lhyM24.jpg)
![lhyKGF.png](https://s2.ax1x.com/2020/01/10/lhyKGF.png)

## Project setup
```
npm install
```

### Compiles and hot-reloads for development
The compilation of WebAssembly code hasn't been added to the build script yet.
You may need to change Emscripten path in  `src/wasm/compile.bat` and run it manually.
```
npm run serve
```

### Compiles and minifies for production
```
npm run build
```

### Lints and fixes files
```
npm run lint
```
