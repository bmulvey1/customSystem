bits = 16;

% file = matfile('division.mat', 'Writable', true);
count = 0;
count1 = 0;
count2 = 0;
results = zeros(2^bits, 2^bits, 'uint16');
disp('start loop')
tic
parfor i = 0:2^bits - 1
    r_par = zeros(1,2^bits);
   for j = 0:1:2^bits - 1
       
       if i == j
          r_par(j+1) = cast(1,'uint16');
          count2 = count2 + 1;
       elseif i < j
           if j < 2*i
               r_par(j+1) = cast(1, 'uint16');
                         count2 = count2 + 1;

           else
               r_par(j+1) = cast(0, 'uint16');
                         count2 = count2 + 1;

           end
       else
           for k = -1:-1:-16
               tmp = bitshift(i, k);
               if tmp == j
                    r_par(j+1) = 2^(-k);
                    count1 = count1 + 1;
                    continue
               elseif tmp - j == j
                   r_par(j+1) = 2^(-k) + 1;
                   count1 = count1 + 1;
               elseif tmp - 2*j == j
                   r_par(j+1) = 2^(-k) + 1;
                   count1 = count1 + 2;
               end
           end
           r_par(j+1) = cast(i/j, 'uint16');
           count = count + 1;
       end
       
       %if i ~= j
       %    tmp = i/j;
       %    if tmp == Inf
       %       tmp = 0; 
       %    end
       %    results(i+1, j+1) = cast(tmp, 'uint16');
       %elseif j <= 2*i
       %    results(i+1, j+1) = cast(1, 'uint16');
       %end
   end
   results(i+1, :) = r_par;
end
toc
