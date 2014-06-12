var xmlHttpota;

function check(num)
{
	xmlHttpota=GetXmlHttpObject();
	if (xmlHttpota==null)
	{
		alert ("Browser does not support HTTP Request");
		Return;
	}
	var url="/cgi-bin/ota.cgi";
	if(num == 1)
	{
		url=url+"?q=check";	
		xmlHttpota.onreadystatechange=stateChanged1;
		widget_hide("otaupload");
	}
	else if(num == 2)
	{
		url=url+"?q=up";
		xmlHttpota.onreadystatechange=stateChanged2;
	}
	
	url=url+"&sid="+Math.random();
	xmlHttpota.open("GET",url,true);
	xmlHttpota.send(null);
}
function stateChanged1()
{
	if (xmlHttpota.readyState==4 || xmlHttpota.readyState=="complete")
	{
		var result = xmlHttpota.responseText;
		//alert(zidong);
		if(result.indexOf("NO NEW SYSTEM!!!") > 0  )
		{
			trans_inner("cres","admw nonewversion");
			widget_hide("otaupload");
		}
		else if(result.indexOf("NEW SYSTEM!!!") > 0  )
		{
			trans_inner("cres","admw newversion");
			widget_display("otaupload");
		}
		else
		{
			trans_inner("cres","admw nonewversion");
			widget_hide("otaupload");
		}
	}
}
function stateChanged2()
{
	if (xmlHttpota.readyState==4 || xmlHttpota.readyState=="complete")
	{
		var result = xmlHttpota.responseText;
		if(result.indexOf("UPDATE SYSTEM!!!") > 0  )
		{
			waitgifshow();
			window.parent.ttzhuan(document.getElementById("IPA").value);
			CheckUser("/tmp/AR9344.bin");
		}
		else
		{
			Butterlate.setTextDomain("admin");
			trans_inner("cres","admw downloaderr");
			window.parent.DialogHide();
		}
	}
}
function GetXmlHttpObject()
{var xmlHttpota=null;try{xmlHttpota=new XMLHttpRequest();}catch (e){try{xmlHttpota=new ActiveXObject("Msxml2.XMLHTTP");}	catch (e){xmlHttpota=new ActiveXObject("Microsoft.XMLHTTP");}}return xmlHttpota;}
