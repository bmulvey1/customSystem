bits = 16;

% file = matfile('division.mat', 'Writable', true);
results = zeros(2^bits, 2^bits, 'uint16');
disp('start loop')
tic
parfor i = 0:2^bits - 1
    r_par = zeros(1,2^bits);
   for j = 0:1:2^bits - 1
       
       if i == j
          r_par(j+1) = cast(1,'uint16');
       elseif i < j
           if j < 2*i
               r_par(j+1) = cast(1, 'uint16');
           else
               r_par(j+1) = cast(0, 'uint16');
           end
       else
           for k = -1:-1:-16
               if bitshift(i, k) == j
                    r_par(j+1) = 2^(-k);
                    continue
               end
           end
           r_par(j+1) = cast(i/j, 'uint16');
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
