#!/usr/bin/awk -f
BEGIN {
  print "encfactor equals " encfactor;

  COLOR_MATCH=0.9;
  COLOR_TAIL=0.8;
  COLOR_TAIL_START=0.6;
  COLOR_P5_STREAK=0.4;
  COLOR_STREAK=0.3;

  RGB[0]="1 1 1";
  RGB[COLOR_MATCH]="0 0 1";
  RGB[COLOR_TAIL]="0.2 0.2 0.2";
  RGB[COLOR_TAIL_START]="0.6 0.6 0.3";
  RGB[COLOR_P5_STREAK]="0.4 0.4 1";
  RGB[COLOR_STREAK]="0.4 0.9 1";
  RGB[0.2]="0.8 0.9 0.8";
  RGB[1]="0.3 0.7 1";

  ENC_ALPHA=0.15;
  ENC_COLOR="1 0 0";

  LOOK_FORWARD=3;
  TAIL_LIMIT=3

  while(getline < encfile) {
    if($1==$2) continue;
    enc_line++;
    enc_start[enc_line]=$1/encfactor;
    enc_end[enc_line]=$2/encfactor;
    for(j=3; j<=6; j++) {
      enc_sets[enc_line,j-2]=($j)+1;
      for(i=$1/encfactor; i<=$2/encfactor; i++) {
        enc[($j)+1,i]=enc_line;
      }
    }
  }
  max_enc_line=enc_line;
  set=0;

}

{
  set++;
  if(max_times < NF)
    max_times = NF;
  for(time = 1; time <= NF; time++) {
    data[set, time] = $time;
    if(data[set,time]!=0) {
      nonzero_set[set]++;
      nonzero_time[time]++;
      if(data[set,time]==1)
        ones_time[time]++;
    }
  }
}

END {
  max_sets = set;

  for(time=1; time<=max_times; time++) {
    streak=0;
    gap_streak=0;
    gap=0;
    max_streak=0;
    max_gap_streak=0;
    for(set=1; set<=max_sets; set++) {
      if(nonzero_set[set]<=max_times/2 && data[set,time]!=0) {
        streak++;
        gap_streak++;
        if(max_streak<streak)
          max_streak=streak;
        if(max_gap_streak<gap_streak)
          max_gap_streak=gap_streak;
      } else {
        if(gap==0) {
          gap=1;
        }
        else {
          gap_streak=streak;
          gap=(streak!=0);
        }
        streak=0;
      }
    }
    strike_streak[time]=(max_streak>=5 || max_gap_streak>=6);
    if(strike_streak[time]) {
      for(set=1; set<=max_sets; set++) {
        if(data[set,time]==1)
          data[set,time]=COLOR_STREAK;
      }
    }
  }


  for(set=1; set<=max_sets; set++) {
    if(nonzero_set[set]>max_times/2)
      continue;
    for(time=1; time<=max_times; time++) {
      i=time;
      p5_streak=0;
      while(data[set,i]!=0) {
        p5_streak++;
        i+=5;
      }
      if(p5_streak>20) {
        for(i=0; i<p5_streak; i++) {
          if(data[set,time+(5*i)]==1)
            ones_time[time+(5*i)]--;
          data[set,time+(5*i)]=COLOR_P5_STREAK;
        }
      }
    }
  }

  for(time=1; time<=max_times; time++) {
    if(ones_time[time]==0)
      continue;
    for(set=1; set<=max_sets; set++) {
      if(data[set,time] == 1) {
        delete forward;
        delete backward;
        for(i=c=1; c<=10; i++) {
          if(time+i>max_times)
            break;
          if(ones_time[time+i]==0)
            continue;
          if(strike_streak[time+i])
            continue;
          if(data[set, time+i]!=1 && ones_time[time+i]<4)
            continue;
          for(j=c; j<=10; j++)
            forward[j]+=(data[set,time+i] >= COLOR_TAIL_START);
          c++;
        }
        for(i=c=1; c<=10; i++) {
          if(time-i<1)
            break;
          if(ones_time[time-i]==0)
            continue;
          if(strike_streak[time-i])
            continue;
          if(data[set, time-i]!=1 && ones_time[time-i]<4)
            continue;
          for(j=c; j<=10; j++)
            backward[j]+=(data[set,time-i] >= COLOR_TAIL_START);
          c++;
        }
        if(forward[LOOK_FORWARD]>0 && backward[LOOK_FORWARD]==0) {
          data[set,time]=COLOR_TAIL_START;
          current_tail[set]=time;
        }
        else if(forward[LOOK_FORWARD]==0 && backward[LOOK_FORWARD]>0) {
          data[set,time]=1;
          tail_start[set,time]=current_tail[set];
        }
        else {
          data[set,time]=COLOR_TAIL;
        }
      }
    }
  }

  for(time=1; time<=max_times; time++) {
    if(ones_time[time]==0)
      continue;
    for(set=1; set<=max_sets; set++) {
      if(data[set,time] == 1 && enc[set,time]>0 && !enc_found[enc[set,time],set]) {
        data[set,time] = COLOR_MATCH;
        match_count++;
        enc_found[enc[set,time],set]=1;
      }
      else if(data[set,time] == 1)
        mismatch_count++;
    }
  }

  for(time=1; time<=max_times; time++) {
    if(ones_time[time]==0)
      continue;
    for(set=1; set<=max_sets; set++) {
      if(data[set,time] == 1) {
        tail_length=time-tail_start[set,time];
        for(i=time-tail_length; i<time+tail_length; i++) {
          if(enc[set,i]>0 && !enc_found[enc[set,i],set]) {
            data[set,time] = COLOR_MATCH;
            match_count++;
            enc_found[enc[set,i],set]=1;
            mismatch_count--;
            break;
          }
        }
      }
    }
  }

  for(set=1; set<=max_sets; set++) {
    for(time=1; time<=max_times; time++) {
      if(ones_time[time]==0 && (ones_time[time-1]==0 || strike_streak[time-1]))
        continue;
      printf("%.1f ",data[set,time]) > prefix "attacker_cleaned";
    }
    printf("\n") > prefix "attacker_cleaned";
  }

  removed=0;
  removed2=0;
  for(time=1; time<=max_times; time++) {
    if(ones_time[time]==0 && ones_time[time-1]==0)
      removed++;
    if(ones_time[time]==0 || strike_streak[time])
      removed2++;
    updated_time[time]=time-removed;
    updated_time2[time]=time-removed2;
  }

  obj=1;
  obj2=1;
  obj3=1;

#  limit1 = 120;
#  limit2 = 60;
  limit1 = 10000;
  limit2 = 10000;

  for(enc_line=1; enc_line<=max_enc_line; enc_line++) {
    b=enc_start[enc_line];
    e=enc_end[enc_line];
    if(b==e) e++;
    sb=int((enc_start[enc_line-1]+b)/2);
    se=(e+b)/2;
    if(se!=int(se)) {
      se=int(se)+1;
      if(se-sb<=1) {
        sb--;
        se++;
      }
    }
    for(t=sb;t<=se;t++) {
      for(table=0;table<4;table++) {
        count[s]=0;
        for(s=1; s<=16; s++) {
          ds=table*16+s;
          if(data[s,t]>0)
            count[s]++;
        }
         
      }
    }
  }

  for(enc_line=1; enc_line<=max_enc_line; enc_line++) {
    for(j=1; j<=4; j++) {
      s=enc_sets[enc_line,j]-1;
      f=enc_found[enc_line,s+1];
      border_color=f?0:2;
      b=enc_start[enc_line];
      e=enc_end[enc_line];
      sb=(enc_start[enc_line-1]+b)/2;
#      se=(e+b)/2;
      se=e;
      if(se-sb<=1) {
        sb--;
        se++;
      }

    b=enc_start[enc_line];
    e=enc_end[enc_line];
    if(b==e) e++;
    sb=int((enc_start[enc_line-1]+b)/2);
    se=(e+b)/2;
    if(se!=int(se)) {
      se=int(se)+1;
      if(se-sb<=1) {
        sb--;
        se++;
      }
    }


      nborder_color=4;
      for(k=int(sb);k<=int(se);k++) {
        if(data[s+1,k]>0) {
          nborder_color=border_color;
          break;
        }
      }
      if(b==e)
        e++;
      printf("set object %d rectangle from %.1f,%.1f to %.1f,%.1f front fillstyle transparent solid 0.1 border %d lw 0.5 fillcolor rgb 'red'\n",
                obj3, b-0.5, s-0.5, e-0.5, s+0.5, border_color) > prefix "victim_default";
      printf("set object %d rectangle from %.1f,%.1f to %.1f,%.1f front fillstyle transparent solid 0.1 border %d lw 0.5 fillcolor rgb 'blue'\n",
                obj3, sb-0.5, s-0.5, se-0.5, s+0.5, nborder_color) > prefix "victim_sync";
      printf("set object %d rectangle from %.1f,%.1f to %.1f,%.1f front fillstyle transparent solid 0.1 border %d lw 0.5 fillcolor rgb 'red'\n",
                obj3, b*encfactor-0.5, s-0.5, e*encfactor-0.5, s+0.5, border_color) > prefix "victim_map";
      obj3++;
    }
    new_start=updated_time[enc_start[enc_line]];
    new_end=updated_time[enc_end[enc_line]];
    new_start2=updated_time2[enc_start[enc_line]];
    new_end2=updated_time2[enc_end[enc_line]];
    if(new_start>limit1 && new_start2>limit2)
      break;
    if(enc_end[enc_line]!=0 && new_end==new_start)
      new_end++;
    if(enc_end[enc_line]!=0 && new_end2==new_start2)
      new_end2++;
    for(j=1; j<=4; j++) {
      s=enc_sets[enc_line,j]-1;
      if(new_start<=limit1)
        printf("set object %d rectangle from %.1f,%.1f to %.1f,%.1f front fillstyle transparent solid 0.1 border 0 lw 0.5 fillcolor rgb 'red'\n",
		obj++, new_start-0.5, s-0.5, new_end-0.5, s+0.5) > prefix "victim_cleaned";
      if(new_start2<=limit2)
        printf("set object %d rectangle from %.1f,%.1f to %.1f,%.1f front fillstyle transparent solid 0.1 border 0 lw 0.5 fillcolor rgb 'red'\n",
		obj2++, new_start2-0.5, s-0.5, new_end2-0.5, s+0.5) > prefix "victim_cleaned2";
    }
  }
  printf("unset for [i=1:%d] object i\n",obj) > prefix "victim_unset";
  printf("unset for [i=1:%d] object i\n",obj2) > prefix "victim_unset2";
  printf("unset for [i=1:%d] object i\n",obj3) > prefix "victim_default_unset";


  split(ENC_COLOR,enc_color);
  for(time=1; time<=max_times; time++) {
    for(set=1; set<=max_sets; set++) {
      split(RGB[data[set, time]],color);
      if(enc[set,time]>0) {
        color[1]=ENC_ALPHA*enc_color[1]+(1-ENC_ALPHA)*color[1];
        color[2]=ENC_ALPHA*enc_color[2]+(1-ENC_ALPHA)*color[2];
        color[3]=ENC_ALPHA*enc_color[3]+(1-ENC_ALPHA)*color[3];
      }
      printf("%d %d %.1f %.1f %.1f\n", time-1, set-1, (1-color[1]), (1-color[2]), (1-color[3])) > prefix "bin";
      if( !(ones_time[time]==0 || strike_streak[time]) )
        printf("%d %d %.1f %.1f %.1f\n", time-1, set-1, (1-color[1]), (1-color[2]), (1-color[3])) > prefix "bin2";

    }
#    printf("\n") > prefix "out";
  }




  printf("matches: %d\n",match_count) > prefix "align_results";
  printf("mismatches: %d\n",mismatch_count) > prefix "align_results";
  printf("remaining: %d\n",4*max_enc_line-match_count) > prefix "align_results";

  x=match_count;
  y=4*max_enc_line-match_count;
  z=mismatch_count;
  a=x+z;
  v=x+y;
  t=64*(x+y+z);
  sigmas=sqrt(a*v*(t-a)*(t-v));
  if(sigmas==0) {
    printf("a=%d, v=%d, t=%d\n",a,v,t) > "/dev/stderr";
    csv=0;
  }
  else {
    csv=(x*(t-x-z-y)-y*z)/sqrt(a*v*(t-a)*(t-v));
  }
  printf("csv: %f\n",csv) > prefix "csv_result";


#  printf("#") > prefix "out";
#  for(time=1; time<=max_times; time++) {
#    printf("%d ", time) > prefix "out";
#  }
#  printf("\n") > prefix "out";
  for(set=1; set<=max_sets; set++) {
    for(time=1; time<=max_times; time++) {
      printf("%.1f ", data[set, time]) > prefix "out";
    }
    printf("\n") > prefix "out";
  }

  for(set=1; set<=max_sets; set++) {
    for(time=1; time<=max_times; time++) {
      if(ones_time[time]==0 || strike_streak[time]) continue;
        printf("%s ", data[set, time]) > prefix "out2";
    }
    printf("\n") > prefix "out2";
  }

  enc_line=1;
  for(time=1; time<=max_times; time++) {
    for(set=1; set<=max_sets; set++) {
      if(time > enc_end[enc_line])
        enc_line++;
      if(ones_time[time]==0)
        continue;
      if(data[set,time]==COLOR_MATCH) {
        printf("%d %d\n", set-1, set-1) > prefix "svf_trace";
      }
      else if(data[set,time]==1) {
        printf("%d %d\n", set-1, -1) > prefix "svf_trace";
      }
    }
    for(i=1;i<=4;i++) {
      s=enc_sets[enc_line,i];
      if(!enc_found[enc_line,s] && !enc_wrote[enc_line,s] ) {
        printf("%d %d\n", -1, s-1) > prefix "svf_trace";
        enc_wrote[enc_line,s]=1;
      }
    }
  }
  

}
