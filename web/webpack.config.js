const path = require('path');

module.exports = {
  entry: {
      // cam: './cam.js',
      control: './control.js',
  },
  output: {
    filename: '[name].js',
    path: path.resolve(__dirname, 'dist'),
  },
  watch: true
};