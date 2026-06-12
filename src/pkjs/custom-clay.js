module.exports = function() {
  var clayConfig = this;

  function updateNumericDependents() {
    var enabled = clayConfig.getItemByMessageKey('NumericTime').get();
    var dependents = ['OneLineTime', 'Hour24Time'];
    dependents.forEach(function(key) {
      var item = clayConfig.getItemByMessageKey(key);
      if (enabled) {
        item.enable();
      } else {
        item.disable();
      }
    });
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    updateNumericDependents();
    clayConfig.getItemByMessageKey('NumericTime').on('change', updateNumericDependents);
  });
};
