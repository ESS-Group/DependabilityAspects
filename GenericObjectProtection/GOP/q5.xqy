(: TODO FIXME: Alle Integer-Vergleiche vorher in xs:integer($val) casten!!! :)

declare variable $repo := doc("repo.acp");

(: ============================= Helper Functions ========================== :)

(: name -> id :)
declare function local:get_ids($functions as xs:string*) as xs:integer* {
  let $ids := for $func in $repo//Function
	      where $func/@name eq $functions
              return $func/@id
  return $ids
};

(: id -> name :)
declare function local:get_names($function_ids as xs:integer*) as xs:string* {
  (: TODO/FIXME: namespaces and virtual functions! :)

  let $non_member := for $func in $repo//Function
	             where $func/@id = $function_ids
                       and $func/@kind <= 2
                     return $func/@name

  let $member := for $func in $repo//Function
	         where $func/@id = $function_ids
                   and $func/@kind > 2
                 return concat( $func/../../@name, "::", $func/@name )

  return distinct-values( ($non_member, $member) )
};

(: id -> cv_qualifiers :)
declare function local:get_cv_qualifiers($function_id as xs:integer) as xs:integer {
  for $func in $repo//Function
    where $func/@id = $function_id
    return $func/@cv_qualifiers
};

(: id -> kind :)
declare function local:get_kind($function_id as xs:integer) as xs:integer {
  for $func in $repo//Function
    where $func/@id = $function_id
    return $func/@kind
};

(: Function node -> result_type :)
declare function local:get_result_type( $function as node() ) as xs:string {
    if ( exists($function/result_type/Type)
         and not(contains($function/result_type/Type/@signature, "?")) )
      then $function/result_type/Type/@signature
    else "&#37;"
};

(: Function node -> argument-types list :)
declare function local:get_arg_types( $function as node() ) as xs:string {
  let $args := for $type in $function/arg_types/Type
               return
                 if ( contains($type/@signature, "?") ) then "&#37;"
                 else $type/@signature

  let $arg_list := if ($function/@variadic_args eq "true") then
                     if ( count($args) eq 0 ) then "..."
                     else concat( string-join( $args , ", " ) , ", ...")
                   else string-join( $args , ", " )

  return
    if ( string-length($arg_list) ne 0 ) then $arg_list
    else ""
};

(: id -> signature :)
declare function local:get_signatures($function_ids as xs:integer*) as xs:string* {
  (: TODO/FIXME: namespaces and virtual functions! :)
  (: WORKAROUND: prepend ...:: for all signatures for safety :)

  let $non_member := for $func in $repo//Function
	             where $func/@id = $function_ids
                       and $func/@kind <= 2
                     return concat( local:get_result_type($func),
                                    " ...::",
                                    $func/@name,
                                    "(", local:get_arg_types($func), ")" )

  let $member := for $func in $repo//Function
	         where $func/@id = $function_ids
                   and $func/@kind > 2
                 return concat( local:get_result_type($func),
                                " ...::",
                                $func/../../@name, "::", $func/@name,
                                "(", local:get_arg_types($func), ")" )

  return distinct-values( ($non_member, $member) )
};


declare function local:pointcut_OR($signature as xs:string*) as xs:string* {
  for $sig in $signature
    return concat("&#34;", $sig, "&#34; &#124;&#124;")
};

(: ========================================================================= :)

(: ========================= Reachability Analysis ========================= :)

(: ids of functions that are not defined within the ac++ project :)
declare function local:get_undefined_functions() as xs:integer* {
  for $func in $repo//Function
    where not($func/source/Source/@kind = 1) (: @kind = 1 => definition :)
      and $func/@kind < 6 (: no constructor or destructor :)
    return $func/@id
};

declare function local:get_pointercalling_functions() as xs:integer* {
  for $func in $repo//Function
    where exists($func/children/CallRef)
    return $func/@id
};


declare function local:reachable($function_ids as xs:integer*) as xs:integer* {
  (: compute all callers of $functions :)
  let $caller := for $call in $repo//Function/children/Call
                 where $call/@target = $function_ids
                 (: returns true if there is at least one element
                    in the sequence that matches :)
                 return $call/../../@id

  (: $function_ids + $callers :)
  let $reachable_ids := distinct-values( ($function_ids, $caller) )

  return
    if ( count($function_ids) = count($reachable_ids) ) then $function_ids
    else local:reachable($reachable_ids)
};

(: ========================================================================= :)

(: ================= GOP Same-Object-And-Block Analysis ==================== :)

declare function local:same_object_and_block( $call1 as node(), $call2 as node() ) as xs:boolean {
  $call1/@target_class eq $call2/@target_class
    and ( exists($call1/@cfg_block_lid) and exists($call2/@cfg_block_lid)
          and ($call1/@cfg_block_lid eq $call2/@cfg_block_lid) )
    and local:get_cv_qualifiers($call1/@target) eq local:get_cv_qualifiers($call2/@target)
    and local:get_kind($call1/@target) eq local:get_kind($call2/@target)
    and ( local:get_kind($call1/@target) eq 4
          or ( exists($call1/@target_object_lid)
               and ($call1/@target_object_lid eq $call2/@target_object_lid) ))
};

declare function local:no_lrf_between( $call1 as node(), $call2 as node(), $lrf as xs:integer* ) as xs:boolean {
  (: $lrf := sequence of ids of long-running functions :)
  let $that   := $call1/../..

  let $lid1 := xs:integer($call1/@lid)
  let $lid2 := xs:integer($call2/@lid)

  let $high_lid := if ($lid1 gt $lid2) then $lid1 else $lid2
  let  $low_lid := if ($lid1 lt $lid2) then $lid1 else $lid2

  let $long_running_calls := for $call in $that/children/Call
                             where xs:integer($call/@lid) gt $low_lid
                               and xs:integer($call/@lid) lt $high_lid
                               and $call/@target = $lrf
                             return $call

  let $call_refs := for $call_ref in $that/children/CallRef
                    where xs:integer($call_ref/@lid) gt $low_lid
                      and xs:integer($call_ref/@lid) lt $high_lid
                    return $call_ref

  return empty($long_running_calls) and empty($call_refs)
};

declare function local:get_next_match( $call_param as node(), $lrf as xs:integer* ) as node()? {
  let $that   := $call_param/../..

  let $result := for $call in $that/children/Call
                 where xs:integer($call/@lid) gt xs:integer($call_param/@lid)
                   and local:same_object_and_block($call_param, $call)
                   and local:no_lrf_between($call_param, $call, $lrf)
                 order by xs:integer($call/@lid) ascending
                 return $call

  return subsequence($result, 1, 1) (: we only need the smallest @lid :)
};

declare function local:get_prev_match( $call_param as node(), $lrf as xs:integer* ) as node()? {
  let $that   := $call_param/../..

  let $result := for $call in $that/children/Call
                 where xs:integer($call/@lid) lt xs:integer($call_param/@lid)
                   and local:same_object_and_block($call_param, $call)
                   and local:no_lrf_between($call_param, $call, $lrf)
                 order by xs:integer($call/@lid) descending
                 return $call

  return subsequence($result, 1, 1) (: we only need the largest @lid :)
};

declare function local:skip_leave( $call_param as node(), $assume_skipped as node()*, $lrf as xs:integer* ) as xs:boolean {
  let $target := $call_param/@target
  let $that   := $call_param/../..

  let $calls_to_target := for $call in $that/children/Call
                          where $call/@target eq $target
                          return $call

  let $can_skip_leave := for $call in $calls_to_target
                         let $call2 := local:get_next_match($call, $lrf)
                         where (not(empty($call2)))
                           and count($calls_to_target) lt 8 (: FIXME: MASSIVE SPEED UP :)
                           and ( ($call2 = $assume_skipped)
                                  or local:skip_enter($call2, ($assume_skipped, $call), $lrf) )
                         return $call

  return ( count($can_skip_leave) eq count($calls_to_target) )
};

declare function local:skip_enter( $call_param as node(), $assume_skipped as node()*, $lrf as xs:integer* ) as xs:boolean {
  let $target := $call_param/@target
  let $that   := $call_param/../..

  let $calls_to_target := for $call in $that/children/Call
                          where $call/@target eq $target
                          return $call

  let $can_skip_enter := for $call in $calls_to_target
                         let $call2 := local:get_prev_match($call, $lrf)
                         where (not(empty($call2)))
                           and count($calls_to_target) lt 8 (: FIXME: MASSIVE SPEED UP :)
                           and ( ($call = $assume_skipped)
                                  or local:skip_leave($call2, ($assume_skipped, $call), $lrf) )
                         return $call/@lid

  return ( count($can_skip_enter) eq count($calls_to_target) )
};

(: ========================================================================= :)

declare function local:main() as xs:string* {
  let $long_running_functions := local:reachable( ( local:get_undefined_functions(),
                                                    local:get_pointercalling_functions(),
                                                    local:get_ids(("hal_thread_switch_context")) ) )
  (: let $long_running_functions := () :)

  let $skip_enter := for $func in $repo//Function,
                     $call in $func/children/Call
                     where exists($func/@id)
                       and local:skip_enter($call, (), $long_running_functions)
                     return concat( "(call(&#34;", local:get_signatures($call/@target), "&#34;)",
                                    " &#38;&#38; within(&#34;", local:get_signatures($func/@id), "&#34;)) &#124;&#124;" )

  let $skip_leave := for $func in $repo//Function,
                     $call in $func/children/Call
                     where exists($func/@id)
                       and local:skip_leave($call, (), $long_running_functions)
                     return concat( "(call(&#34;", local:get_signatures($call/@target), "&#34;)",
                                    " &#38;&#38; within(&#34;", local:get_signatures($func/@id), "&#34;)) &#124;&#124;" )


  let $newline := '&#10;'
  let $hashtag := '&#35;'
  let $quote   := '&#34;'
  let $comment_start := "&#47;&#42;"
  let $comment_end := "&#42;&#47;"

  let $header := concat( $hashtag, "ifndef __GOP_STATIC_OPTIMIZATION__AH_", $newline, 
                         $hashtag, "define __GOP_STATIC_OPTIMIZATION__AH_", $newline, $newline,
                         $comment_start, " auto-generated file: do not edit ", $comment_end, $newline, $newline,
                         "aspect GOP_Static_Optimization {", $newline,
                         "public: " )

  let $footer := concat ( "};", $newline, $hashtag, "endif", $newline )

  let $empty_call_set := concat ( "call(", $quote, "None* no::does::not::match(None*)", $quote, ");" )

  return ( $header,
           "pointcut shortFunctions() = &#34;&#37; ...::&#37;(...)&#34; &#38;&#38; !(",
             local:pointcut_OR( local:get_signatures ( $long_running_functions ) ),
             "&#34;None* no::does::not::match(None*)&#34;);",
           $newline,
           "pointcut skip_enter() = ", $skip_enter, $empty_call_set,
           $newline,
           "pointcut skip_leave() = ", $skip_leave, $empty_call_set,
           $newline,
           $comment_start,
           "shortFunctions(): ", xs:string(count($long_running_functions)),
           "skip_enter(): ", xs:string(count($skip_enter)),
           "skip_leave(): ", xs:string(count($skip_leave)),
           $comment_end,
           $footer )
};

local:main()

