#!/bin/bash
#
# gitlog2changelog bash script
# v1.00, 2013-07-18
#
# Copyright 2013       Raphaël Beamonte <raphael.beamonte@gmail.com>
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License Version
# 2 as published by the Free Software Foundation.
#
# gitlog2changelog is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
# or see <http://www.gnu.org/licenses/>.
#

#####################################################################
#####################################################################
# Command line options
OPT_TAGREGEX=
while getopts ":ho:r:" opt; do
	case $opt in
		# Show help
		h)
			echo "usage: $0 [options]" >&2
			echo "	-h		Show this message" >&2
			exit 0
			;;
		# Redirect output
		o)
		if ([ -f "$OPTARG" ] && [ -w "$OPTARG" ]) || [ -w "$(dirname "$OPTARG")" ]; then
				exec > $OPTARG
			else
				echo "error: he file '$OPTARG' is not writeable." >&2
				exit 1
			fi
			;;
		# Regex to match tag with to be used as version
		r)
			OPT_TAGREGEX="$OPTARG"
			;;
		# Unknown option
		\?)
			echo "Invalid option: -$OPTARG" >&2
			echo "Use -h for help." >&2
			exit 1
			;;
		# Argument needed
		:)
			echo "Option -$OPTARG requires an argument." >&2
			echo "Use -h for help." >&2
			exit 1
			;;
	esac
done

#####################################################################
#####################################################################

#
# Used to format as GNU asks, showing date and author before
# each concerned ChangeLog line.
#
GDATE=
GAUTHOR=
show_dateauthor() {
	local LAUTHOR=$(git show -s --format='%aN  <%aE>' ${1})
	local LDATE=$(git show -s --format='%ai' ${1} | cut -d' ' -f1)
	if [ "$LDATE" != "$GDATE" ] || [ "$LAUTHOR" != "$GAUTHOR" ]; then
		if [ -n "$GDATE" ]; then
			echo ""
		fi
		GDATE=$LDATE
		GAUTHOR=$LAUTHOR
		echo "$GDATE  $GAUTHOR"
	fi
}

#
# Reset the GNU date+author format when changing tag
#
reset_show_dateauthor() {
	GDATE=
	GAUTHOR=
}

#
# Function separating and formating each commit for one tag
#
show_commits() {
	reset_show_dateauthor
	echo "$1" | sed "s/\\\\/\\\\\\\\/g" | while read line; do
		show_dateauthor $(echo "$line" | cut -d' ' -f1)
		echo "$line" \
			| sed -r "s/^([a-z0-9]{7}) (.*)$/\t* \2 [\1]/g" \
			| fold -s -w70 \
			| sed '2,$s/^/\t  /'
	done
}

#
# Function used to format a tag header
#
tagversion() {
	local line="[$1]: $(git tag -l $1 -n10 | head -n1 | cut -f2- -d' ' | sed "s/^[ ]*//g")"
	echo "$line"
	echo $(printf %$(echo "$line" | grep -o '.' | wc -l)s | tr " " "=")
}

#
# Header of the ChangeLog file
#
echo "#"
echo "# ChangeLog generated on $(date +"%Y-%m-%d, %H:%M:%S")"
echo "#"
## To generate one Copyright line per author
while read author; do
	# Format the name of the author correctly, we use the most recent commit
	AUTHOR=$(git log --author="^$author <[^ ]*>$" --format="format:%aN <%aE>" | head -n1)
	# Get the first commit date
	DATEFROM=$(git log --author="^$author <[^ ]*>$" --format="format:%ai" | tail -n1 | cut -d'-' -f1)
	# Get the last commit date
	DATETO=$(git log --author="^$author <[^ ]*>$" --format="format:%ai" | head -n1 | cut -d'-' -f1)
	# Format the Copyright line
	if [ "$DATEFROM" = "$DATETO" ]; then DATE="$DATEFROM     ";
	else DATE="$DATEFROM-$DATETO"; fi
	echo "[sort:$DATETO-$DATEFROM]: # Copyright $DATE  $AUTHOR"
done < <(git shortlog | grep "^[^ ]" | sed "s/([0-9]*):$//g") | sort -r | cut -d' ' -f2-
echo "# Copying and distribution of this file, with or without modification, are"
echo "# permitted provided the copyright notice and this notice are preserved."
echo "#"
echo "#"
echo "# WALL OF BLAME (per text file line)"
echo "# ----------------------------------"
git ls-tree -r HEAD \
	| sed -re 's/^.{53}//' \
	| xargs echo -e \
	| xargs file \
	| grep ': .*text$' \
	| sed -r -e 's/: .*//' \
	| xargs --max-args=1 git --no-pager blame -w HEAD \
	| sed -r -e 's/.*\((.*)[0-9]{4}-[0-9]{2}-[0-9]{2} .*/\1/' -e 's/ +$//' \
	| sort \
	| uniq -c \
	| sort -nr \
	| sed 's/^/# /'
echo "#"

#
# Routine to separate per tags (used as versions)
#
PTAG=
shopt -s nocasematch
while read tag; do
	if [ -n "${OPT_TAGREGEX}" ]; then
		if [[ ! "$tag" =~ ^${OPT_TAGREGEX}$ ]]; then
			# If tag does not match asked regex,
			# we just go directly to the next tag
			continue
		fi
	fi
	if [ -z "$PTAG" ]; then
		#
		# If we have commits unversionned
		#
		COMMITS="$(git log --oneline ${tag}.. 2>&1)"
		if [ -n "$COMMITS" ]; then
			echo ""
			echo ""
			echo "Not versioned:"
			echo "=============="
			show_commits "$COMMITS"
		fi
	else
		#
		# All the commits between our current tag and the next one
		#
		COMMITS="$(git log --oneline ${tag}..${PTAG} 2>&1)"
		echo ""
		echo ""
		tagversion $PTAG
		show_commits "$COMMITS"
	fi
	PTAG=$tag
done < <(git log --oneline --decorate | egrep "^[a-z0-9]{7} \([^)]*tag: " | grep -o "tag: [^),]*" | cut -c6-)
shopt -u nocasematch

#
# For the first tag, we use all the commits before
#
if [ -n "$PTAG" ]; then
	COMMITS="$(git log --oneline ${PTAG} 2>&1)"
	echo ""
	echo ""
	tagversion $PTAG
	show_commits "$COMMITS"
fi

