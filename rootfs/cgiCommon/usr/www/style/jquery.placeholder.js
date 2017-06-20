/*
* @author ambar
* html5 placeholder pollfill
* - 使用绝对定位内嵌层
* - 也适用于密码域
* 目标浏览器: IE 6~9, FF 3.5

```
// 默认
$('input[placeholder]').placeholder()

// autofocus 与 placeholder 搭配时，如要防止清空了提示文本(safari 6 & ff 16 已不存在此问题)，推荐
$('input[placeholder]').placeholder({
  // focus 时不清除提示文本， keypress 有效字符时才清空
  hideOnFocus: false
})
```
*/
(function ($) {
  var attr = 'placeholder', nativeSupported = attr in document.createElement('input')

  $.fn.placeholder = function (options) {
    return this.each(function () {
      var $input = $(this)

      if ( typeof options === 'string' ) {
        options = { text: options }
      }

      var opt = $.extend({
        text     : '',
        style    : {},
        namespace: 'placeholder',
        hideOnFocus: true
      }, options || {})

      if ( !opt.text ) {
        opt.text = $input.attr(attr)
      }

      if ( nativeSupported ) {
        // 仅改变文本 
        $input.attr(attr, opt.text)
        return
      }

      var width     = $input.width(), height = $input.height()
      var box_style = ['marginTop', 'marginLeft', 'paddingTop', 'paddingLeft', 'paddingRight']
      
      var show      = function () { $layer.show() }
      var hide      = function () { $layer.hide() }
      var is_empty  = function () { return !$input.val() }
      var check     = function () { is_empty() ? show() : hide() }
      
      var position  = function () {
        var pos = $input.position()
        if (!opt.hideOnFocus) {
          // 按鍵隱藏的情况，需要移動光標两像素
          pos.left += 2
        }
        $layer.css(pos)
        $.each(box_style, function (i, name) {
          $layer.css(name, $input.css(name))
        })
      }

      var layer_style = {
          color     : 'gray',
          cursor    : 'text',
          textAlign : 'left',
          position  : 'absolute',
          textShadow: 'none',
          fontSize  : $input.css('fontSize'),
          fontFamily: $input.css('fontFamily'),
          display   : is_empty() ? 'block' : 'none'
      }
      
      // create
      var layer_props = {
        text  : opt.text,
        width : width,
        height: 'auto'
      }

      // 确保只绑定一次
      var ns = '.' + opt.namespace, $layer = $input.data('layer' + ns)
      if (!$layer) {
        $layer = $('<label>', layer_props)
          .attr('for', $input.attr('id'))
          .appendTo($input.offsetParent())
        
        $input.data('layer' + ns, $layer)
      }

      // activate
      $layer
      .css($.extend(layer_style, opt.style))
      .unbind('click' + ns)
      .bind('click' + ns, function () {
        opt.hideOnFocus && hide()
        $input.focus()
      })

      $input
      .unbind(ns)
      .bind('blur' + ns, check)

      if (opt.hideOnFocus) {
        $input.bind('focus' + ns, hide)
      }else{
        $input.bind('keypress keydown' + ns, function(e) {
          var key = e.keyCode
          if (e.charCode || (key >= 65 && key <=90)) {
            hide()
          }
        })
        .bind('keyup' + ns,check)
      }

      // 由于 ie 记住密码的特性，需要监听值改变
      // ie9 不支持 jq bind 此事件
      $input.get(0).onpropertychange = check

      position()
      check()
    })
  }

})(jQuery)