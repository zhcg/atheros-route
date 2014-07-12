function getCookie() {
    var strCookie = document.cookie;
    var arrCookie = strCookie.split("; ");
    for (var i = 0; i < arrCookie.length; i++) {
        var arr = arrCookie[i].split("=");
        if (arr[0] == "userName") {
            return true
        }
    }
    return false
}
function style_display_on() {
    if (window.ActiveXObject) {
        return "inline-block"
    } else if (window.XMLHttpRequest) {
        return ""
    }
}
function widget_hide(widgetID) {
    document.getElementById(widgetID).style.visibility = "hidden";
    document.getElementById(widgetID).style.display = "none"
}
function widget_display(widgetID) {
    document.getElementById(widgetID).style.visibility = "visible";
    document.getElementById(widgetID).style.display = style_display_on()
}
function widget_enable(widgetID) {
    document.getElementById(widgetID).disabled = false
}
function widget_disable(widgetID) {
    document.getElementById(widgetID).disabled = true
}
function trans_inner(widgetID, xmlname) {
    var e = document.getElementById(widgetID);
    e.innerHTML = _(xmlname)
}
function trans_value(widgetID, xmlname) {
    var e = document.getElementById(widgetID);
    e.value = _(xmlname)
}
function button_commit(buttonID) {
    document.getElementById(buttonID).click()
}
function atoi(str, num) {
    i = 1;
    if (num != 1) {
        while (i != num && str.length != 0) {
            if (str.charAt(0) == '.') {
                i++
            }
            str = str.substring(1)
        }
        if (i != num) return - 1
    }
    for (i = 0; i < str.length; i++) {
        if (str.charAt(i) == '.') {
            str = str.substring(0, i);
            break
        }
    }
    if (str.length == 0) return - 1;
    return parseInt(str, 10)
}
function checkRange(str, num, min, max) {
    d = atoi(str, num);
    if (d > max || d < min) return false;
    return true
}
function isAllNum(str) {
    for (var i = 0; i < str.length; i++) {
        if ((str.charAt(i) >= '0' && str.charAt(i) <= '9') || (str.charAt(i) == '.')) continue;
        return 0
    }
    return 1
}
function checkIpAddr(field, ismask) {
    if (field.value == "") {
        if (ismask == 1) {
            alert(_("err IP empty"))
        } else if (ismask == 2) {
            alert(_("err sub empty"))
        } else if (ismask == 3) {
            alert(_("err gw empty"))
        } else if (ismask == 4) {
            alert(_("err DNS empty"))
        }
        field.value = field.defaultValue;
        field.focus();
        return false
    }
    if (isAllNum(field.value) == 0) {
        if (ismask == 1) {
            alert(_("err IP format"))
        } else if (ismask == 2) {
            alert(_("err sub format"))
        } else if (ismask == 3) {
            alert(_("err gw format"))
        } else if (ismask == 4) {
            alert(_("err DNS format"))
        }
        field.value = field.defaultValue;
        field.focus();
        return false
    }
    if (ismask == 1) {
        if ((!checkRange(field.value, 1, 0, 255)) || (!checkRange(field.value, 2, 0, 255)) || (!checkRange(field.value, 3, 0, 255)) || (!checkRange(field.value, 4, 1, 254))) {
            alert(_("err IP format"));
            field.value = field.defaultValue;
            field.focus();
            return false
        }
    } else if (ismask == 2) {
        if ((!checkRange(field.value, 1, 0, 255)) || (!checkRange(field.value, 2, 0, 255)) || (!checkRange(field.value, 3, 0, 255)) || (!checkRange(field.value, 4, 0, 255))) {
            alert(_("err sub format"));
            field.value = field.defaultValue;
            field.focus();
            return false
        }
    } else if (ismask == 3) {
        if ((!checkRange(field.value, 1, 0, 255)) || (!checkRange(field.value, 2, 0, 255)) || (!checkRange(field.value, 3, 0, 255)) || (!checkRange(field.value, 4, 1, 254))) {
            alert(_("err gw format"));
            field.value = field.defaultValue;
            field.focus();
            return false
        }
    } else if (ismask == 4) {
        if ((!checkRange(field.value, 1, 0, 255)) || (!checkRange(field.value, 2, 0, 255)) || (!checkRange(field.value, 3, 0, 255)) || (!checkRange(field.value, 4, 1, 254))) {
            alert(_("err DNS format"));
            field.value = field.defaultValue;
            field.focus();
            return false
        }
    } {
        var exp = /^(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$/;
        var reg = field.value.match(exp);
        if (reg == null) {
            if (ismask == 1) {
                alert(_("err IP format"))
            } else if (ismask == 2) {
                alert(_("err sub format"))
            } else if (ismask == 3) {
                alert(_("err gw format"))
            } else if (ismask == 4) {
                alert(_("err DNS format"))
            }
            field.value = field.defaultValue;
            field.focus();
            return false
        }
    }
    return true
}
function checktimerange(value, min, max) {
    var i = parseInt(value);
    if (isNaN(i)) {
        return false
    } else if (i > max || i < min) {
        return false
    }
    return true
}
function isNum(str) {
    for (var i = 0; i < str.length; i++) {
        if ((str.charAt(i) >= '0' && str.charAt(i) <= '9')) continue;
        return false
    }
    return true
}
function Checkhanzi(val) {
    var usern = /^[a-zA-Z0-9_@]{1,}$/;
    if (!val.match(usern)) return false;
    else return true
}
function fucCheckNUM(NUM)
{
	var i,j,strTemp;
	strTemp="0123456789";
	if ( NUM.length== 0)
		return 0
	for (i=0;i<NUM.length;i++)
	{
		j=strTemp.indexOf(NUM.charAt(i)); 
		if (j==-1)
		{
		//说明有字符不是数字
		return 0;
		}
	}
	//说明是数字
	return 1;
}
function checkValuerange(value, min, max) 
{
	if(fucCheckNUM(value) == 0)
		return false;
    var i = parseInt(value);
    if (isNaN(i)) {
        return false;
    } else if (i > max || i < min) {
        return false;
    }
    return true;
}
function showpwd(spanID, passName, passID, checkboxID) {
    var pass = document.getElementById(passID).value;
    if (document.getElementById(checkboxID).checked == true) {
        document.getElementById(spanID).innerHTML = '<input type="text" name=' + passName + ' id=' + passID + ' maxlength="32" size="32"  class="handaer_text_content" value="' + pass + '" />';
    } else {
        document.getElementById(spanID).innerHTML = '<input type="password" name=' + passName + ' id=' + passID + ' maxlength="32" size="32"  class="handaer_text_content" value="' + pass + '" />';
    }
}
function checkInjection(str) {
    var len = str.length;
    for (var i = 0; i < str.length; i++) {
        if (str.charAt(i) == '\r' || str.charAt(i) == '\n') {
            return false;
        } else continue;
    }
    return true;
}
function Checkformat(val) {
    var usern = /^[a-zA-Z0-9_]{1,}$/;
    if (!val.match(usern)) return false;
    else return true;
}
function ip2int(ip) {
    var num = 0;
    ip = ip.split(".");
    num = Number(ip[0]) * 256 * 256 * 256 + Number(ip[1]) * 256 * 256 + Number(ip[2]) * 256 + Number(ip[3]);
    num = num >>> 0;
    return num;
}
function hzcheck(val) {
    var usern = /^[a-zA-Z0-9.]{1,}$/;
    if (!val.match(usern)) return false;
    else return true;
}