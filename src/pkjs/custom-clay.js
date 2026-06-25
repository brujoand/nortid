module.exports = function() {
  var clayConfig = this;

  // "3" is the Numbers language: it renders exact HH:MM digits, so the 24-hour
  // option applies to it as well as to the NumericTime toggle.
  var LANG_NUMERIC = '3';

  function numericActive() {
    return clayConfig.getItemByMessageKey('NumericTime').get();
  }

  function languageIsNumeric() {
    return clayConfig.getItemByMessageKey('Language').get() === LANG_NUMERIC;
  }

  function setEnabled(key, enabled) {
    var item = clayConfig.getItemByMessageKey(key);
    if (enabled) {
      item.enable();
    } else {
      item.disable();
    }
  }

  function updateDependents() {
    // OneLineTime only applies to the NumericTime toggle; the Numbers language
    // is always laid out on one line already.
    setEnabled('OneLineTime', numericActive());
    setEnabled('Hour24Time', numericActive() || languageIsNumeric());
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    updateDependents();
    clayConfig.getItemByMessageKey('NumericTime').on('change', updateDependents);
    clayConfig.getItemByMessageKey('Language').on('change', updateDependents);
  });
};
