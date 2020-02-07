
module.exports = {
  "transpileDependencies": [
    "vuetify"
  ],
  publicPath: "./",
  configureWebpack: {
    module: {
      rules: [
        {
          test: /dewarp\.js$/,
          loader: "exports-loader"
        },
        {
          test: /dewarp\.wasm$/,
          type: "javascript/auto",
          loader: "file-loader",
        }
      ]
    },
  }
};