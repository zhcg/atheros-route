var httpversion;

function getversion()
{
	httpversion=GetObject();
	if (httpversion==null)
	{
		alert ("Browser does not support HTTP Request");
		Return;
	}
	var url="http://"+location.hostname+"/version.xml?now="+ new Date().getTime();

	httpversion.onreadystatechange=getversioncb;
	httpversion.open("GET",url,true);
	httpversion.send(null);
}
function getversioncb()
{
	if (httpversion.readyState==4 || httpversion.readyState=="complete")
	{
		var result = httpversion.responseText;
		//alert(zidong);
		if((result.indexOf("V1.0.") >= 0)||(result.indexOf("V8.0.") >= 0))
		{
			widget_display("denglukuang_img");
			widget_display("note");
		}
	}
}

function GetObject()
{var httpversion=null;try{httpversion=new XMLHttpRequest();}catch (e){try{httpversion=new ActiveXObject("Msxml2.XMLHTTP");}	catch (e){httpversion=new ActiveXObject("Microsoft.XMLHTTP");}}return httpversion;}
