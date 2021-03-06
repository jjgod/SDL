<?PHP
	include ("../include/login.inc.php");
	include ("header.inc.php");

	/**
	 * The letter l (lowercase L) and the number 1 have been removed,
	 * as they can be mistaken for each other.
	 */
	function create_password() {
		$chars = "abcdefghijkmnopqrstuvwxyz023456789";
		srand((double)microtime()*1000000);
		$i = 0;
		$pass = '' ;

		while ($i <= 7) {
			$num = rand() % 33;
			$tmp = substr($chars, $num, 1);
			$pass = $pass . $tmp;
			$i++;
		}
		return $pass;
	}

	BeginContent("Users");

	$fields_def = array(
		'id'=>array('type'=>'integer', 'required'=>True),
		'groupid'=>array('type'=>'integer', 'required'=>True),
		'login'=>array('type'=>'char', 'size'=>20, 'required'=>True),
		'name'=>array('type'=>'char', 'size'=>64, 'required'=>True),
		'email'=>array('type'=>'char', 'size'=>64, 'required'=>False),
		'reset'=>array('type'=>'char', 'size'=>64, 'required'=>True),
	);

	switch ($action) {
		case "createuser":
			echo <<<EOT
<form method="post" action="{$_SERVER['PHP_SELF']}?action=insertuser">
<i>All fields are required</i><br>
<p>Login (case sensitive)<br><input type="text" name="login" value="{$_GET['login']}" size="16" maxlength="20"></p>
<p>Name<br><input type="text" name="name" value="{$_GET['name']}" size="50" maxlength="64"></p>
<p>Email<br><input type="text" name="email" value="{$_GET['email']}" size="50" maxlength="64"></p>
<p><input type="submit" value="Submit"></p>
</form>
<p>Note that the information submitted here is kept completely private.</p>

EOT;
			break;

		case "insertuser":
			# --- check user input ---

			$input = validateinput($_POST, $fields_def,
				array('login', 'name', 'email'));
			if (!$input)
				break;

			# --- make sure there is a valid e-mail address provided

			if (!strchr($input['email'], '@')) {
			echo <<<EOT
<p>
You must provide a valid e-mail address.
</p>
<p>
This address is never publicly visible, and is only used to recover your password and by the SDL administrators to contact you if it ever becomes necessary.
</p>
<p>
Click <a href="users.php?action=createuser&amp;login={$input['login']}&amp;name={$input['name']}">here</a> to re-enter your e-mail address.
</p>
EOT;
				break;
			}


			# --- check the login is not already used by someone else ---

			$query = "select login from users where login='{$input['login']}'";
			$result = mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			if (mysql_num_rows($result) != 0) {
				print "That login is used by someone else. Please choose another one...<br>\n";
				break;
			}

			# --- insert him in the user table ---

			$password = create_password();
			$passhash = md5($password);

			$query = "insert into users (groupid,login,password,name,email,created)
				values(2,'{$input['login']}','$passhash','{$input['name']}','{$input['email']}',CURRENT_TIMESTAMP)";
			mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			# --- mail user ---

			$mail_login = $input['login'];
			$mail_password = $password;
			include("../include/mail-newuser.inc.php");
			mail($input['email'], $subject, $body, "from: $from");

			# --- mail webmaster ---

			mail(WEBADMIN, "New user registered on libsdl.org !", "login: {$input['login']}\nname: {$input['name']}\nemail: {$input['email']}\n", "from: ".WEBADMIN);

			print "You are now registered and your password has been mailed to the address you provided. Click <a href=\"index.php?action=showloginform\">here</a> to login !\n";
			break;

		case "resetpwd":
			# --- check user input ---

			$input = validateinput($_POST, $fields_def, array('reset'));
			if (!$input)
				break;

			if (strchr($input['reset'], '@')) {
				$query = "select login, email from users where email='{$input['reset']}'";
			} else {
				$query = "select login, email from users where login='{$input['reset']}'";
			}
			$result = mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			if (mysql_num_rows($result) == 0) {
				print "That login or e-mail address is not valid, please <a href=\"index.php?action=showresetform\">re-enter</a> the one you want to reset...<br>\n";
				break;
			}
			$login = mysql_result($result, 0, 'login');
			$email = mysql_result($result, 0, 'email');

			# --- reset the password ---

			$password = create_password();
			$passhash = md5($password);

			$query = "update users set password='$passhash' where login = '$login'";
			mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			# --- mail user ---

			$mail_login = $login;
			$mail_password = $password;
			include("../include/mail-changedpassword.inc.php");
			mail($email, $subject, $body, "from: $from");

			print "Your new password has been mailed to the address you provided. Click <a href=\"index.php?action=showloginform\">here</a> to login !\n";
			break;

		case "changepwd":
			$input = validateinput($_GET, $fields_def, array('id'));
			if (!$input)
				break;
				
			if (!$userprivileges['manageusers'])
				if (($userid != $input['id']) || ($userid < 1)) {
					print "You are not permitted to access this page !<br>\n";
					break;
				}

			# --- get his login ---

			$query = "select login from users where id={$input['id']}";
			$result = mysql_query($query, $DBconnection)
				or die ("Could not execute query !");
			$login = mysql_result($result, 0, "login");

			$oldpasswordline = !$userprivileges['manageusers'] ? 
				'<p>old password<br><input type="password" name="oldpass" size="25" maxlength="100"></p>' :
				'';
			echo <<<EOT
<form method="post" action="{$_SERVER['PHP_SELF']}?action=updatepwd&amp;id={$input['id']}">
<p><i>change password for $login</i></p>
$oldpasswordline
<p>new password<br><input type="password" name="pass1" size="25" maxlength="100"></p>
<p>new password<br><input type="password" name="pass2" size="25" maxlength="100"></p>
<p><input type="submit" value="Submit"></p>
</form>
<a href="users.php?action=edituser&amp;id={$input['id']}">back</a>

EOT;
			break;

		case "updatepwd": 
			$input = validateinput($_GET, $fields_def, array('id'));
			if (!$input)
				break;
			
			$id = $input['id'];
				
			# Note that some variables are initialized in login.inc

			# --- check everything is valid ---

			if (!$userprivileges['manageusers']) {
				if (($userid != $input['id']) || ($userid < 1)) {
					print "You are not permitted to access this page !<br>\n";
					break;
				}

				if ($oldpassword != $userpassword) {
					print "Wrong password !<br>\n";
					break;
				}
			}

			if ($_POST['pass1'] == "") {
				print "You may not use an empty password !<br>\n";
				break;
			}

			if ($_POST['pass1'] != $_POST['pass2']) {
				print "Your confirmation password is not the same than your password !<br>\n";
				break;
			}

			# --- update his password ---

			$query = "update users set password='$newpassword' where id=$id";
			mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			# --- mail user ---

			# --- get his email address ---

			$query = "select login, email from users where id=$id";
			$result = mysql_query($query, $DBconnection)
				or die ("Could not execute query !");
			$login = mysql_result($result, 0, 'login');
			$email = mysql_result($result, 0, 'email');

			# --- send the mail ---

			$mail_login = $login;
			$mail_password = $_POST['pass1'];
			include("../include/mail-changedpassword.inc.php");
			mail($email, $subject, $body, "from: $from");

			echo <<<EOT
<i>Updated !</i><br>
<br>
<a href="users.php?action=edituser&amp;id=$id">back</a>

EOT;
			break;

		case "updateuser":
			$input = validateinput($_GET, $fields_def, array('id'));
			if (!$input)
				break;
			
			$id = $input['id'];
			
			if (!$userprivileges['manageusers'])
				if (($userid != $id) || ($userid < 1)) {
					print "You are not permitted to access this page !<br>\n";
					break;
				}

			# --- check his data is valid ---

			$field_list = array('name', 'email');
			if ($userprivileges['manageusers'])
				$field_list[] = 'groupid';
				
			$input = validateinput($_POST, $fields_def, $field_list);
			if (!$input)
				break;
				
			# --- update his infos ---

			if ($userprivileges['manageusers'])
				$query = "update users set groupid={$input['groupid']}, name='{$input['name']}', email='{$input['email']}' where id=$id";
			else
				$query = "update users set name='{$input['name']}', email='{$input['email']}' where id=$id";

			mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			print "<i>Updated !</i><br>\n";
			
			# no break ! after updating, we go back the the edit screen

		case "edituser":
			$input = validateinput($_GET, $fields_def, array('id'));
			if (!$input)
				break;
			
			$id = $input['id'];
			
			if (!$userprivileges['manageusers'])
				if (($userid != $id) || ($userid < 1)) {
					print "You are not permitted to access this page !<br>\n";
					break;
				}

			# fetch his current infos

			$query = "select * from users where id=$id";
			$result = mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			$row = mysql_fetch_array($result, MYSQL_ASSOC);

			# compute some variable interface elements

			$group_sel = '';
			$delete_account_form = '';
			
			if ($userprivileges['manageusers']) {
				$groupid = $row['groupid'];

				$query = "select id, name from groups where id>=0";
				$result = mysql_query($query, $DBconnection)
					or die ("Could not execute query !");

				$group_sel .= '<p>group<br><select name="groupid">';
				while ($group_row = mysql_fetch_array($result, MYSQL_ASSOC))
					$group_sel .= OPTIONSTR($group_row['id'], $groupid, $group_row['name']);
				$group_sel .= "</select></p>\n";
			} else {
				$delete_account_form = <<<EOT
<form method="post" action="{$_SERVER['PHP_SELF']}?action=removeuser&amp;id=$id">
<input type="submit" value="delete account">
</form>

EOT;
			}

			$back_dest = ($userid != $id) ? "users.php" : "index.php";

			# display the form(s)

			echo <<<EOT
<p><b>login</b><br><i>{$row['login']}</i></p>

<form method="post" action="{$_SERVER['PHP_SELF']}?action=changepwd&amp;id=$id">
<p>password<br><input type="submit" value="change"></p>
</form>

<form method="post" action="{$_SERVER['PHP_SELF']}?action=updateuser&amp;id=$id">
<p>name<br><input type="text" value="{$row['name']}" name="name" size="25" maxlength="100"></p>
<p>email<br><input type="text" value="{$row['email']}" name="email" size="25" maxlength="100"></p>
$group_sel
<p><input type="submit" value="submit"></p>
</form>

$delete_account_form

<a href="$back_dest">back</a>
EOT;
			break;

		case "removeuser":
			$input = validateinput($_GET, $fields_def, array('id'));
			if (!$input)
				break;
			
			$id = $input['id'];
			
			if (!$userprivileges['manageusers'])
				if (($userid != $id) || ($userid <= 0)) {
					print "You are not permitted to access this page !<br>\n";
					break;
				}

			$query = "select login from users where id=$id";
			$result = mysql_query($query, $DBconnection)
				or die ("Could not execute query !");
			$login = mysql_result($result, 0, "login");

			# if the user himself tried to delete his account, go back to
			# his account edit screen, if it's someone else, go back to the
			# user list
			$cancel_action = ($userid == $id) ? "?action=edituser&amp;id=$id" : "";

			echo <<<EOT
Are you sure you want to delete $login account?<br>
<br>
<table>
<tr>
<td>
<form method="post" action="{$_SERVER['PHP_SELF']}?action=deleteuser&amp;id=$id">
<input type="submit" value="delete">
</form>
</td>
<td>
<form method="post" action="users.php$cancel_action">
<input type="submit" value="cancel">
</form>
</td>
</tr>
</table>
EOT;
			break;

		case "deleteuser":
			$input = validateinput($_GET, $fields_def, array('id'));
			if (!$input)
				break;
			
			$id = $input['id'];

			if (!$userprivileges['manageusers'])
				if (($userid != $id) || ($userid < 1)) {
					print "You are not permitted to access this page !<br>\n";
					break;
				}

			# --- set his news items' author to the special 'deleted' user ---

			$query = "update news set userid=-1 where userid=$id";
			mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			# --- set his projects' maintainer to the special 'deleted' user ---

			$query = "update projects set userid=-1 where userid=$id";
			mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			# --- remove him from user list ---

			$query = "delete from users where id=$id";
			mysql_query($query, $DBconnection)
				or die ("Could not execute query !");
			
			$back_dest = ($userid != $id) ? "users.php" : "index.php";
			
			echo <<<EOT
Deleted !<br>
<br>
<a href="$back_dest">back</a>
EOT;
			break;

		default: # list users
			if (!$userprivileges['manageusers']) {
				print "You are not permitted to access this page !<br>\n";
				break;
			}

			# --- fetch users infos ---

			$query  = "select users.id as id, login, users.name as username, email, groups.name as groupname ";
			$query .= "from users,groups ";
			$query .= "where (users.groupid=groups.id) and (users.id>0) order by username";
			$result = mysql_query($query, $DBconnection)
				or die ("Could not execute query !");

			# --- print users infos ---

			echo '<table cellpadding="4">', "\n";

			while ($row = mysql_fetch_array($result, MYSQL_ASSOC))
				echo <<<EOT
<tr>
	<td>{$row['username']}</td>
	<td><a href="mailto:{$row['email']}">{$row['email']}</a></td>
	<td>{$row['groupname']}</td>
	<td><a href="{$_SERVER['PHP_SELF']}?action=edituser&amp;id={$row['id']}">edit</a></td>
	<td><a href="{$_SERVER['PHP_SELF']}?action=removeuser&amp;id={$row['id']}">delete</a></td>
</tr>
EOT;

			print "</table>\n";
	}

	CloseContent();

	include ("footer.inc.php");
?>
