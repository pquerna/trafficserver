<!-------------------------------------------------------------------------
  ------------------------------------------------------------------------->

<@include /include/header.ink>
<@include /configure/c_header.ink>

<form method=POST action="/submit_update.cgi?<@link_query>">
<input type=hidden name=record_version value=<@record_version>>
<input type=hidden name=submit_from_page value=<@link_file>>

<table width="100%" border="0" cellspacing="0" cellpadding="0">
  <tr class="tertiaryColor"> 
    <td class="greyLinks"> 
      <p>&nbsp;&nbsp;<@record proxy.config.manager_name> UI Access Configuration</p>
    </td>
  </tr>
</table>

<@include /configure/c_buttons_hide.ink>

<@submit_error_msg>

<table width="100%" border="0" cellspacing="0" cellpadding="10"> 
  <tr>
    <td width="100%" nowrap class="configureLabel" valign="top">
       <@submit_error_flg proxy.config.admin.ip_allow.filename>Access Control
    </td>
  </tr>
  <tr>
    <td width="100%" class="configureHelp" valign="top" align="left"> 
      The "<@record proxy.config.admin.ip_allow.filename>" file lets
      you specify the IP addresses of the remote hosts allowed to
      access the <@record proxy.config.manager_name> UI.
      If no entry exists in the file, no remote hosts are allowed
      access.
    </td>
  </tr>
  <tr>
   <td>
    <@config_table_object /configure/f_mgmt_allow_config.ink>
   </td>
  </tr>
  <tr>
    <td colspan="2" align="right">
     <input class="configureButton" type=button name="refresh" value="Refresh" onclick="window.location='/configure/c_mgmt_access.ink?<@link_query>'">
     <input class="configureButton" type=button name="editFile" value="Edit File" target="displayWin" onclick="window.open('/configure/submit_config_display.cgi?filename=/configure/f_mgmt_allow_config.ink&fname=<@record proxy.config.admin.ip_allow.filename>&frecord=proxy.config.admin.ip_allow.filename', 'displayWin');">
    </td>
  </tr>
</table>

<@include /configure/c_buttons_hide.ink>
<@include /configure/c_footer.ink>

</form>

<@include /include/footer.ink>

