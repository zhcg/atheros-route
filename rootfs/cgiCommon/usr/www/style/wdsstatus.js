var wdshttp_2G;
var wdshttp_5G;
var timer_2G;
var timer_5G;
var times_2G = 0;
var times_5G = 0;

function wdsstatuscheck(num)
{
	if(num == 1)
	{
		wdshttp_2G=GetWDShttp();
		if (wdshttp_2G==null)
		{
			alert ("Browser does not support HTTP Request");
			Return;
		}
		var url="/cgi-bin/search.cgi";
		url=url+"?q=2GCHECK";

		url=url+"&sid="+Math.random();
		wdshttp_2G.onreadystatechange=status_2G;
		wdshttp_2G.open("GET",url,true);
		wdshttp_2G.send(null);
	}
	else if(num == 2)
	{
		wdshttp_5G=GetWDShttp();
		if (wdshttp_5G==null)
		{
			alert ("Browser does not support HTTP Request");
			Return;
		}
		var url="/cgi-bin/search.cgi";
		url=url+"?q=5GCHECK";

		url=url+"&sid="+Math.random();
		wdshttp_5G.onreadystatechange=status_5G;
		wdshttp_5G.open("GET",url,true);
		wdshttp_5G.send(null);
	}
}
function status_2G()
{
	if (wdshttp_2G.readyState==4 || wdshttp_2G.readyState=="complete")
	{
		var result1 = wdshttp_2G.responseText;
		
		if(result1.indexOf("none") >= 0 )
		{
			document.getElementById("w1_status").innerHTML=_("admw wds0");
			times_2G ++ ;
			if(times_2G >= 30)
			{
				clearInterval(timer_2G);
				document.getElementById("w1_status").innerHTML=_("admw wds3");
			}
		}
		else if(result1.indexOf("unnormal") >= 0 )
		{
			document.getElementById("w1_status").innerHTML=_("admw wds2");
			clearInterval(timer_2G);
		}		
		else if(result1.indexOf("normal") >= 0 )
		{
			document.getElementById("w1_status").innerHTML=_("admw wds1");
			clearInterval(timer_2G);
		}

		else if(result1.indexOf("invalid") >= 0 )
		{
			document.getElementById("w1_status").innerHTML=_("admw wds4");
			clearInterval(timer_2G);
		}
		else
		{
			document.getElementById("w1_status").innerHTML=_("admw wds0");
		
		}

	}
}
function status_5G()
{
	if (wdshttp_5G.readyState==4 || wdshttp_5G.readyState=="complete")
	{
		var result2 = wdshttp_5G.responseText;
		
		if(result2.indexOf("none") >= 0 )
		{
			document.getElementById("w2_status").innerHTML=_("admw wds0");
			times_5G ++ ;
			if(times_5G >= 30)
			{
				clearInterval(timer_5G);
				document.getElementById("w2_status").innerHTML=_("admw wds3");
			}
		}
		else if(result2.indexOf("unnormal") >= 0 )
		{
			document.getElementById("w2_status").innerHTML=_("admw wds2");
			clearInterval(timer_5G);
		}
		else if(result2.indexOf("normal") >= 0 )
		{
			document.getElementById("w2_status").innerHTML=_("admw wds1");
			clearInterval(timer_5G);
		}

		else if(result2.indexOf("invalid") >= 0 )
		{
			document.getElementById("w2_status").innerHTML=_("admw wds4");
			clearInterval(timer_5G);
		}
		else
		{
			document.getElementById("w2_status").innerHTML=_("admw wds0");
		}
	}
}
function GetWDShttp()
{var httpp=null;try{httpp=new XMLHttpRequest();}catch (e){try{httpp=new ActiveXObject("Msxml2.XMLHTTP");}	catch (e){httpp=new ActiveXObject("Microsoft.XMLHTTP");}}return httpp;}
